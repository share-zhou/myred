/*
 * net/sched/sch_myred.c	RBF-PID using Gradient descent method.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Junlong Qiao, <zheolong@126.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include "/root/AQM/myred/include/myred.h"
#include "/root/AQM/myred/include/myred_queue.h"
#include <asm/i387.h>   //to support the Floating Point Operation
#include <linux/rwsem.h>
#include<linux/kthread.h>
#include<linux/interrupt.h>
//#include "tp-myred-trace.h"//trace debug
//#include "tp-myred-vars-trace.h"//trace debug
//DEFINE_TRACE(myred_output);//trace debug
//DEFINE_TRACE(myred_vars_output);//trace debug
//#define TRACE_COUNT_MAX 10
//int trace_count;

//Queue Length Statistics
#define CYC_MAX 20
int cyc_count;

struct task_struct *tsk_prob;
struct task_struct *tsk_stop_prob;


#ifndef SLEEP_MILLI_SEC  
#define SLEEP_MILLI_SEC(nMilliSec)\
do {\
long timeout = (nMilliSec) * HZ / 1000;\
while(timeout > 0)\
{\
timeout = schedule_timeout(timeout);\
}\
}while(0);
#endif

/*	Random Early Detection (RED) algorithm.
	=======================================

	Source: Qiao Junlong, "Random Early Detection Gateways
	for Congestion Avoidance", 1993, IEEE/ACM Transactions on Networking.

	This file codes a "divisionless" version of RED algorithm
	as written down in Fig.17 of the paper.

	Short description.
	------------------

	When a new packet arrives we calculate the average queue length:

	avg = (1-W)*avg + W*current_queue_len,

	W is the filter time constant (chosen as 2^(-Wlog)), it controls
	the inertia of the algorithm. To allow larger bursts, W should be
	decreased.

	if (avg > th_max) -> packet marked (dropped).
	if (avg < th_min) -> packet passes.
	if (th_min < avg < th_max) we calculate probability:

	Pb = max_P * (avg - th_min)/(th_max-th_min)

	and mark (drop) packet with this probability.
	Pb changes from 0 (at avg==th_min) to max_P (avg==th_max).
	max_P should be small (not 1), usually 0.01..0.02 is good value.

	Parameters, settable by user:
	-----------------------------

	limit		- bytes (must be > qth_max + burst)

	Hard limit on queue length, should be chosen >qth_max
	to allow packet bursts. This parameter does not
	affect the algorithms behaviour and can be chosen
	arbitrarily high (well, less than ram size)
	Really, this limit will never be reached
	if RED works correctly.
 */

struct myred_sched_data {
	struct timer_list 	ptimer;	

	u32			limit;		/* HARD maximal queue length */
	unsigned char		flags;

	struct myred_parms	parms;
	struct myred_stats	stats;
	struct Qdisc		*qdisc;
};

struct queue_show queue_show_base_myred[QUEUE_SHOW_MAX];EXPORT_SYMBOL(queue_show_base_myred);
int array_element_myred = 0;EXPORT_SYMBOL(array_element_myred);

static void __inline__ myred_mark_probability(struct Qdisc *sch);


static inline int myred_use_ecn(struct myred_sched_data *q)
{
	return q->flags & TC_MYRED_ECN;
}

static inline int myred_use_harddrop(struct myred_sched_data *q)
{
	return q->flags & TC_MYRED_HARDDROP;
}

static int myred_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct myred_sched_data *q = qdisc_priv(sch);
    struct myred_parms *parms = &q->parms;
	struct Qdisc *child = q->qdisc;
	int ret;

	switch (myred_action(&q->parms)) {
	case MYRED_DONT_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		if(cyc_count==CYC_MAX){
			queue_show_base_myred[array_element_myred].length=sch->q.qlen;
			queue_show_base_myred[array_element_myred].numbers=array_element_myred;
			queue_show_base_myred[array_element_myred].mark_type=MYRED_DONT_MARK;
			queue_show_base_myred[array_element_myred].p=*((long long *)(&parms->proba));
			queue_show_base_myred[array_element_myred].q_min=parms->q_min;
			queue_show_base_myred[array_element_myred].q_max=parms->q_max;
			queue_show_base_myred[array_element_myred].p_min=*((long long *)(&parms->p_min));
			queue_show_base_myred[array_element_myred].p_max=*((long long *)(&parms->qavg));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用
			parms->qavg = (parms->qavg * parms->qcnt + sch->q.qlen) / (++(parms->qcnt));
			printk("<1>%lld\n",*((long long *)(&parms->qavg)));

			if(array_element_myred < QUEUE_SHOW_MAX-1)  array_element_myred++;
			cyc_count = 0;
		}

		break;

	case MYRED_PROB_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		if(cyc_count==CYC_MAX){
			queue_show_base_myred[array_element_myred].length=sch->q.qlen;
			queue_show_base_myred[array_element_myred].numbers=array_element_myred;
			queue_show_base_myred[array_element_myred].mark_type=MYRED_PROB_MARK;
			queue_show_base_myred[array_element_myred].p=*((long long *)(&parms->proba));
			queue_show_base_myred[array_element_myred].q_min=parms->q_min;
			queue_show_base_myred[array_element_myred].q_max=parms->q_max;
			queue_show_base_myred[array_element_myred].p_min=*((long long *)(&parms->p_min));
			queue_show_base_myred[array_element_myred].p_max=*((long long *)(&parms->p_max));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用
			parms->qavg = (parms->qavg * parms->qcnt + sch->q.qlen) / (++(parms->qcnt));

			if(array_element_myred < QUEUE_SHOW_MAX-1)  array_element_myred++;
			cyc_count = 0;
		}

		sch->qstats.overlimits++;
		if (!myred_use_ecn(q) || !INET_ECN_set_ce(skb)) {
			q->stats.prob_drop++;
			goto congestion_drop;
		}

		q->stats.prob_mark++;
		break;
	}

	ret = qdisc_enqueue(skb, child);

//--------------------------------------------------------------------------------------------
	if (likely(ret == NET_XMIT_SUCCESS)) {
		sch->q.qlen++;

		sch->qstats.backlog += skb->len;/*2012-1-21*/
		//sch->qstats.bytes += skb->len;/*2012-1-21*/
		//sch->qstats.packets++;/*2012-1-21*/

	} else if (net_xmit_drop_count(ret)) {
		q->stats.pdrop++;
		sch->qstats.drops++;
	}
	

	return ret;

congestion_drop:
	qdisc_drop(skb, sch);
	return NET_XMIT_CN;
}

static struct sk_buff *myred_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb;
	struct myred_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	skb = child->dequeue(child);
	if (skb) {
		qdisc_bstats_update(sch, skb);

		sch->qstats.backlog -= skb->len;/*2012-1-21*/	

		sch->q.qlen--;
	} else {
	}
	return skb;
}

static struct sk_buff *myred_peek(struct Qdisc *sch)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	return child->ops->peek(child);
}

static unsigned int myred_drop(struct Qdisc *sch)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;
	unsigned int len;

	if (child->ops->drop && (len = child->ops->drop(child)) > 0) {

		sch->qstats.backlog -= len;/*2012-1-21*/					

		q->stats.other++;
		sch->qstats.drops++;
		sch->q.qlen--;
		return len;
	}

	return 0;
}

static void myred_reset(struct Qdisc *sch)
{
	struct myred_sched_data *q = qdisc_priv(sch);

	qdisc_reset(q->qdisc);

	sch->qstats.backlog = 0;/*2012-1-21*/	
	
	sch->q.qlen = 0;

	array_element_myred = 0;/*2012-1-21*/
	
	myred_restart(&q->parms);
}

static void myred_destroy(struct Qdisc *sch)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	
	//删除计时器，并将输出数据需要的array_element_myred置为0
    array_element_myred = 0;
/*
	if(tsk_prob!=NULL)
		kthread_stop(tsk_prob);
	if(tsk_stop_prob!=NULL)
		kthread_stop(tsk_stop_prob);
		*/
	kernel_fpu_end();

	qdisc_destroy(q->qdisc);
}

static const struct nla_policy myred_policy[TCA_MYRED_MAX + 1] = {
	[TCA_MYRED_PARMS]	= { .len = sizeof(struct tc_myred_qopt) },
	[TCA_MYRED_STAB]	= { .len = MYRED_STAB_SIZE },
};

/*qjl
缩写的一些说明：
Qdisc    		Queue discipline
myred      		myred method
nlattr   		net link attributes
myred_sched_data   myred scheduler data
qdisc_priv           qdisc private（Qdisc中针对特定算法如MYRED的数据）
tca			traffic controll attributes
nla 			net link attributes
*/
static int myred_change(struct Qdisc *sch, struct nlattr *opt)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_MYRED_MAX + 1];
	struct tc_myred_qopt *ctl;
	struct Qdisc *child = NULL;
	int err;

	if (opt == NULL)
		return -EINVAL;

	err = nla_parse_nested(tb, TCA_MYRED_MAX, opt, myred_policy);
	if (err < 0)
		return err;

	if (tb[TCA_MYRED_PARMS] == NULL ||
	    tb[TCA_MYRED_STAB] == NULL)
		return -EINVAL;
	
	/*求有效载荷的起始地址*/
	ctl = nla_data(tb[TCA_MYRED_PARMS]);

	if (ctl->limit > 0) {
		child = fifo_create_dflt(sch, &bfifo_qdisc_ops, ctl->limit);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	sch_tree_lock(sch);
	q->flags = ctl->flags;
	q->limit = ctl->limit;
	if (child) {
		qdisc_tree_decrease_qlen(q->qdisc, q->qdisc->q.qlen);
		qdisc_destroy(q->qdisc);
		q->qdisc = child;
	}

	//设置算法参数，此函数是在myred.h中定义的
	myred_set_parms(&q->parms, ctl->sampl_period, 
								 ctl->p_init, ctl->p_min, ctl->p_max, 
								 ctl->q_min, ctl->q_max, 
								 ctl->Scell_log, nla_data(tb[TCA_MYRED_STAB]));

	//利用trace debug内核代码
	//trace_myred_output(ctl);


	array_element_myred = 0;/*2012-1-21*/

	sch_tree_unlock(sch);

	return 0;
}

int myred_thread_func(void* data)
{
	struct Qdisc *sch=(struct Qdisc *)data;
	do{
		sch=(struct Qdisc *)data;
		myred_mark_probability(sch);
		//SLEEP_MILLI_SEC(5);
		schedule_timeout(20*HZ);
	}while(!kthread_should_stop());
	return 0;
}
int myred_stop_prob_thread_func(void* data)
{
	SLEEP_MILLI_SEC(50000);
	kthread_stop(tsk_prob);
	printk("<1>tsk prob stop");
	return 0;
}

static void __inline__ myred_mark_probability(struct Qdisc *sch)
{
    struct myred_sched_data *data = qdisc_priv(sch);
    struct myred_parms *parms = &data->parms;

	//trace debug
	//struct trace_myred_parms trace_parms;

//--------------------------MYRED算法丢弃/标记概率更新过程------------------------
    if (sch->q.qlen < parms->q_min)
        parms->proba = parms->p_min;
	else if (sch->q.qlen > parms->q_max)
        parms->proba = 1;
	else 
		parms->proba = parms->p_max*(parms->qavg-parms->q_min)/(parms->q_max-parms->q_min); 	

    if (parms->proba < parms->p_min)
         parms->proba = parms->p_min;
    if (parms->proba > 1)
         parms->proba = 1;
	printk(KERN_INFO "%lld\n",*((long long *)&(parms->proba)));
//--------------------------------------------------------------------------------
}


static int myred_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	int ret;

	kernel_fpu_begin();

	array_element_myred = 0;/*2012-1-21*/

	cyc_count = 0;

	q->qdisc = &noop_qdisc;
	ret=myred_change(sch, opt);

	tsk_prob=kthread_run(myred_thread_func,(void*)sch,"myred");
	tsk_stop_prob=kthread_run(myred_stop_prob_thread_func,(void*)sch,"myred_stop_prob");

	return ret;
}

static int myred_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	struct nlattr *opts = NULL;
	struct tc_myred_qopt opt = {
		.limit		= q->limit,
		.sampl_period	= q->parms.sampl_period,
		.p_max		= q->parms.p_max,
		.p_min		= q->parms.p_min,
		.p_init		= q->parms.p_init,
		.q_max		= q->parms.q_max,
		.q_min		= q->parms.q_min,
	};

	sch->qstats.backlog = q->qdisc->qstats.backlog;
	opts = nla_nest_start(skb, TCA_OPTIONS);
	if (opts == NULL)
		goto nla_put_failure;
	NLA_PUT(skb, TCA_MYRED_PARMS, sizeof(opt), &opt);
	return nla_nest_end(skb, opts);

nla_put_failure:
	nla_nest_cancel(skb, opts);
	return -EMSGSIZE;
}

static int myred_dump_stats(struct Qdisc *sch, struct gnet_dump *d)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	struct tc_myred_xstats st = {
		.early	= q->stats.prob_drop + q->stats.forced_drop,
		.pdrop	= q->stats.pdrop,
		.other	= q->stats.other,
		.marked	= q->stats.prob_mark + q->stats.forced_mark,
	};

	return gnet_stats_copy_app(d, &st, sizeof(st));
}

static int myred_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	struct myred_sched_data *q = qdisc_priv(sch);

	tcm->tcm_handle |= TC_H_MIN(1);
	tcm->tcm_info = q->qdisc->handle;
	return 0;
}

static int myred_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct myred_sched_data *q = qdisc_priv(sch);

	if (new == NULL)
		new = &noop_qdisc;

	sch_tree_lock(sch);
	*old = q->qdisc;
	q->qdisc = new;
	qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
	qdisc_reset(*old);
	sch_tree_unlock(sch);
	return 0;
}

static struct Qdisc *myred_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct myred_sched_data *q = qdisc_priv(sch);
	return q->qdisc;
}

static unsigned long myred_get(struct Qdisc *sch, u32 classid)
{
	return 1;
}

static void myred_put(struct Qdisc *sch, unsigned long arg)
{
}

static void myred_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
	if (!walker->stop) {
		if (walker->count >= walker->skip)
			if (walker->fn(sch, 1, walker) < 0) {
				walker->stop = 1;
				return;
			}
		walker->count++;
	}
}

static const struct Qdisc_class_ops myred_class_ops = {
	.graft		=	myred_graft,
	.leaf		=	myred_leaf,
	.get		=	myred_get,
	.put		=	myred_put,
	.walk		=	myred_walk,
	.dump		=	myred_dump_class,
};

static struct Qdisc_ops myred_qdisc_ops __read_mostly = {
	.id		=	"myred",
	.priv_size	=	sizeof(struct myred_sched_data),
	.cl_ops	=	&myred_class_ops,
	.enqueue	=	myred_enqueue,
	.dequeue	=	myred_dequeue,
	.peek		=	myred_peek,
	.drop		=	myred_drop,
	.init		=	myred_init,
	.reset		=	myred_reset,
	.destroy	=	myred_destroy,
	.change	=	myred_change,
	.dump		=	myred_dump,
	.dump_stats	=	myred_dump_stats,
	.owner		=	THIS_MODULE,
};

static int __init myred_module_init(void)
{
	return register_qdisc(&myred_qdisc_ops);
}

static void __exit myred_module_exit(void)
{
	unregister_qdisc(&myred_qdisc_ops);
}

module_init(myred_module_init)
module_exit(myred_module_exit)

MODULE_LICENSE("GPL");
