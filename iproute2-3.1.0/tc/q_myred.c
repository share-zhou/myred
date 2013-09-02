/*
 * q_myred.c		MYRED.
 *
 *		This program is free software; you can myredistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Junlong Qiao, <zheolong@126.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

#include "tc_myred.h"
static void explain(void)
{
	fprintf(stderr, "Usage: ... myred limit BYTES sampl_period INTEGER  q_ref PACKETS p_init PROBABILITY p_min PROBABILITY p_max PROBABILITY\n");
	fprintf(stderr, "                eta_p COEFFICIENT eta_i COEFFICIENT eta_d COEFFICIENT\n");	
	fprintf(stderr, "                n INTEGER m INTEGER alpha COEFFICIENT  eta COEFFICIENT\n");	
	fprintf(stderr, "                [ ecn ]\n");
}


static int myred_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	struct tc_myred_qopt opt;
	int ecn_ok = 0;
	__u8 sbuf[256];
	struct rtattr *tail;

	int sampl_period, q_ref;
	double p_init, p_min, p_max, q_min, q_max;
	
	memset(&opt, 0, sizeof(opt));

	/* qjl */
	sampl_period = 1;
	q_ref = 300;
	p_init = 0;
	p_min = 0;
	p_max = 1;
	q_min = 0;
	q_max = 1;
	

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_size(&opt.limit, *argv)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "p_init") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lf", &opt.p_init) != 1) {
				fprintf(stderr, "Illegal \"p_init\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "p_min") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lf", &opt.p_min) != 1) {
				fprintf(stderr, "Illegal \"p_min\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "p_max") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lf", &opt.p_max) != 1) {
				fprintf(stderr, "Illegal \"p_max\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "q_min") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%d", &opt.q_min) != 1) {
				fprintf(stderr, "Illegal \"q_min\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "q_max") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%d", &opt.q_max) != 1) {
				fprintf(stderr, "Illegal \"q_max\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "sampl_period") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%d", &opt.sampl_period) != 1) {
				fprintf(stderr, "Illegal \"sampl_period\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "ecn") == 0) {
			ecn_ok = 1;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}
	
	/*其中有些元素本来就是1，所以这个条件判断有问题*/
//	if (!opt.limit || !opt.q_ref || !opt.qold || !opt.a || !opt.b || !opt.proba || !opt.proba_init || !opt.proba_max || !opt.sampl_period ) {
	//	fprintf(stderr, "Requimyred parameter (limit, q_ref, qold, a, b, proba, proba_init, proba_max, sampl_period) is missing\n");
	//	return -1;
	//}

	if (p_init < 0 || p_init > 1)
		fprintf(stderr, "MYRED: the init probability must be between 0.0 and 1.0.\n");
	if (p_max < 0 || p_max > 1)
		fprintf(stderr, "MYRED: the max probability must be between 0.0 and 1.0.\n");
	if (p_min < 0 || p_min > 1)
		fprintf(stderr, "MYRED: the min probability must be between 0.0 and 1.0.\n");
	if (p_min > p_max)
		fprintf(stderr, "MYRED: the min probability must be less than max probability.\n");

	if (ecn_ok) {
#ifdef TC_MYRED_ECN
		opt.flags |= TC_MYRED_ECN;
#else
		fprintf(stderr, "MYRED: ECN support is missing in this binary.\n");
		return -1;
#endif
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_MYRED_PARMS, &opt, sizeof(opt));
	addattr_l(n, 1024, TCA_MYRED_STAB, sbuf, 256);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int myred_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_MYRED_STAB+1];
	struct tc_myred_qopt *qopt;


	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_MYRED_STAB, opt);

	if (tb[TCA_MYRED_PARMS] == NULL)
		return -1;
	qopt = RTA_DATA(tb[TCA_MYRED_PARMS]);
	if (RTA_PAYLOAD(tb[TCA_MYRED_PARMS])  < sizeof(*qopt))
		return -1;
	fprintf(f, "limit %s",
		sprint_size(qopt->limit, b1));
#ifdef TC_MYRED_ECN
	if (qopt->flags & TC_MYRED_ECN)
		fprintf(f, "ecn ");
#endif
	/*qjl*/
	if (show_details) {
		fprintf(f, "p_init %lf p_min %lf p_max %lf q_min %d q_max %d sampl_period %d",
			qopt->p_init, qopt->p_min, qopt->p_max, qopt->q_min, qopt->q_max, qopt->sampl_period);
	}
	return 0;
}

static int myred_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
#ifdef TC_MYRED_ECN
	struct tc_myred_xstats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, "  marked %u early %u pdrop %u other %u",
		st->marked, st->early, st->pdrop, st->other);
	return 0;

#endif
	return 0;
}


struct qdisc_util myred_qdisc_util = {
	.id		= "myred",
	.parse_qopt	= myred_parse_opt,
	.print_qopt	= myred_print_opt,
	.print_xstats	= myred_print_xstats,
};
