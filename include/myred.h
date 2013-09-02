/*qjl*/
#ifndef __NET_SCHED_MYRED_H
#define __NET_SCHED_MYRED_H

#include <linux/types.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include <net/dsfield.h>
#include <asm/i387.h>   //为了支持浮点运算
/* The largest number rand will return (same as INT_MAX).  */
#define	RAND_MAX	2147483647

/*	MYRED algorithm.
	=======================================

*/
#define MYRED_STAB_SIZE	256
#define MYRED_STAB_MASK	(MYRED_STAB_SIZE - 1)

struct myred_stats {
	u32		prob_drop;	/* Early probability drops */
	u32		prob_mark;	/* Early probability marks */
	u32		forced_drop;	/* Forced drops, qavg > max_thresh */
	u32		forced_mark;	/* Forced marks, qavg > max_thresh */
	u32		pdrop;          /* Drops due to queue limits */
	u32		other;          /* Drops due to drop() calls */
};

struct myred_parms {
	/* Parameters */
	int 	sampl_period;   //采样时间
	//队列控制
	double p_init;//初始丢弃概率
	double p_min;//最小丢弃概率
	double p_max;//最大丢弃概率

	int q_min;//队列长度域值下限
	int q_max;//队列长度域值上限
		
	/* Variables */
	//队列控制
	double 	proba;	/* Packet marking probability */
	//MYRED
	int 	qcnt;	/* Number of packets since last random*/
	double 	qavg;	/* average packets number*/

	u32		Scell_max;
	u8		Scell_log;
	u8		Stab[MYRED_STAB_SIZE];
};

static inline void myred_set_parms(struct myred_parms *p, int sampl_period, 
                             double p_init, double p_min, double p_max, 
                             int q_min, int q_max,
				 u8 Scell_log, u8 *stab)
{
	/* Reset average queue length, the value is strictly bound
	 * to the parameters below, reseting hurts a bit but leaving
	 * it might result in an unreasonable qavg for a while. --TGR
	 */
	p->sampl_period	= sampl_period;

	/* Parameters */
	p->p_init		= p_init;//初始丢弃概率
	p->p_min		= p_min;//最小丢弃概率
	p->p_max		= p_max;//最大丢弃概率

	p->q_min		= q_min;//队列长度域值下限
	p->q_max		= q_max;//队列长度域值上限

	/* Variables */
	//队列控制
	p->proba		= p_init;	/* Packet marking probability */
	//MYRED
	p->qcnt		= 0;	/* Number of packets since last random*/
	p->qavg		= 0.00;	/* average packets number*/

	p->Scell_log	= Scell_log;
	p->Scell_max	= (255 << Scell_log);

	memcpy(p->Stab, stab, sizeof(p->Stab));
}

static inline void myred_restart(struct myred_parms *p)
{
	//待定？？？？
}


/*-------------------------------------------------*/

enum {
	MYRED_BELOW_PROB,
	MYRED_ABOVE_PROB,
};

static inline int myred_cmp_prob(struct myred_parms *p)
{
	int p_random,current_p;
	p_random = abs(net_random());

	//p->p_k will be written by another thread, so when reading it's value in a diffent thread, "current_p_sem" should be set
	current_p = (int)(p->proba*RAND_MAX);
		
	if ( p_random < current_p){
		return MYRED_BELOW_PROB;
	}
	else{
		return MYRED_ABOVE_PROB;
	}
}

enum {
	MYRED_DONT_MARK,
	MYRED_PROB_MARK,
};

static inline int myred_action(struct myred_parms *p)
{
	switch (myred_cmp_prob(p)) {
		case MYRED_ABOVE_PROB:
			return MYRED_DONT_MARK;

		case MYRED_BELOW_PROB:
			return MYRED_PROB_MARK;
	}

	BUG();
	return MYRED_DONT_MARK;
}

#endif
