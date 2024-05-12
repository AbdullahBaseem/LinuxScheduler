#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#define NUM_THREADS 4
#define MAX_SLEEP_AVG 10
#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))
#define filename "./process_list.txt"

struct process_struct{
	int pid;
	int SP;
	int DP;
	char policy[6];
	int affinity;
	int expected_time;
	int remain_time;
	int time_slice;
	int accu_time_slice;
	int sleep_avg;
	struct timeval time_at_block;
	struct timeval time_at_run;
};

struct ready_queue{
	int num_processes; //number of processes in the queue
	struct process_struct process[5];
};

struct consumer_queues{
	int current_RQ;
	int num_RR;
	int num_FIFO;
	int num_NORMAL;
	int total_processes;
	struct ready_queue RQ[3];
};

struct ready_queue sort_RQ(struct ready_queue RQ);
struct ready_queue remove_process(struct ready_queue RQ, int index);
int random_num(int a, int b);
void *producer_function(void *arg);
void *consumer_function(void *arg);

