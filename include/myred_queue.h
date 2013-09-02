/*qjl*/

#define QUEUE_SHOW_MAX 30000

struct queue_show{
           int numbers;
           __u32 length;
		   int mark_type; //MYRED_DONT_MARK or MYRED_PROB_MARK
		   long long p;
		   int q_min;
		   int q_max;
		   long long p_min;
		   long long p_max;
		   long long qavg;
};
extern struct queue_show queue_show_base_myred[QUEUE_SHOW_MAX];
extern int array_element_myred;
extern struct queue_show queue_show_base1[30000];
extern int array_element1;
