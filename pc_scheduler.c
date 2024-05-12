#include "header.h"

//Sort the ready queue from highest priorty (lowest SP value) to lowest priority (highest SP value)
struct ready_queue sort_RQ(struct ready_queue RQ){
	int i, j;
	struct process_struct process;
	for(i = 1; i < RQ.num_processes; i++){
		process = RQ.process[i];
		j = i-1;
		while(j >= 0 && RQ.process[j].DP > process.DP){
			RQ.process[j+1] = RQ.process[j];
			j--;
		}
		RQ.process[j+1] = process;
	}
	return RQ;
}

//"remove" a process by moving it to the back of the queue, and reducing process count so it is never touched
struct ready_queue remove_process(struct ready_queue RQ, int index){
	RQ.process[index].DP = 1000;
	RQ = sort_RQ(RQ);
	RQ.num_processes--;
	return RQ;
}

//Generate random number between a and b
int random_num(int a, int b){
	return ((rand() % b) + a);
}

struct consumer_queues all_queues[4]; //One set of RQs for each processor
pthread_mutex_t lock[4][3]; //One mutex for each RQ
int thread_finished;

int main(){
	int res;
	pthread_t p_thread;
	pthread_t c_thread[NUM_THREADS];
	void *thread_result;
	pthread_attr_t thread_attr;
	int thread_count;
	srand(time(0)); //Initialize seed for random number generation
	
	//Initialize thread attribute
	res = pthread_attr_init(&thread_attr);
	if (res != 0) {
	perror("Attribute creation failed");
	exit(EXIT_FAILURE);
	}
	
	//Set thread attribute as detatched
	res = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	if (res != 0) {
	perror("Setting detached attribute failed");
	exit(EXIT_FAILURE);
	}
	
	//create producer thread
	res = pthread_create(&p_thread, &thread_attr, producer_function, NULL);
	if (res != 0){
		perror("Thread creation failed");
		exit(EXIT_FAILURE);
	}
	
	sleep(1);

	// create consumer threads
	for(thread_count = 0; thread_count < NUM_THREADS; thread_count++) {
		res = pthread_create(&(c_thread[thread_count]), &thread_attr, consumer_function, (void *)&thread_count);
		if (res != 0) {
		    perror("Thread creation failed");
		    exit(EXIT_FAILURE);
		}
		sleep(1);
	}
	
	(void)pthread_attr_destroy(&thread_attr);
	while(thread_finished < 5){}
	printf("[SYSTEM] All consumer queues empty. Threads finished.\n");
	exit(EXIT_SUCCESS);
}

void *producer_function(void *arg){
	int i, x, y; //Counters used for various loops
	int sleep_time; //Random amount (ms) for processor to sleep after getting info for 5 processes
	int process_count = 0; //To track how many processes have been read so far
	int num_processes; //Variable to set when need to index through RQ0,RQ1, or RQ2
	printf("[PRODUCER] Producer thread started...\n");
	//Initialize consumer queues
	for(i = 0; i < NUM_THREADS; i++){
		all_queues[i].current_RQ = 0;
		all_queues[i].num_RR = 0;
		all_queues[i].num_FIFO = 0;
		all_queues[i].num_NORMAL = 0;
		all_queues[i].total_processes = 0;
		//Initialize number of processes in each queue as 0
		for(x = 0; x < 3; x++){
			pthread_mutex_lock(&lock[i][x]);
			all_queues[i].RQ[x].num_processes = 0;
			pthread_mutex_unlock(&lock[i][x]);
		}
	}
	printf("[PRODUCER] Producer reading file and putting processes in queues...\n");
	struct process_struct process;
	FILE *fp;
	fp = fopen(filename, "r");
	char current_line[256];
	if (fp != NULL){
		srand(time(0)); //Initialize seed for random number generation
		//While not at end of file
		while (fgets(current_line, 256, fp) != NULL){
			//Get process info
			sscanf(current_line, "%d %s %d %d %d", &process.pid, process.policy, &process.SP, &process.expected_time, &process.affinity);

			//Assign time slice for RR and NORMAL processes (for FIFO, time_slice = expected_time because it runs to completion)
			if(strcmp(process.policy, "RR") == 0) process.time_slice = (140 - process.SP) * 20;
			else if(strcmp(process.policy, "NORMAL") == 0){
				if(process.SP < 120) process.time_slice = (140 - process.SP) * 20;
				else process.time_slice = (140 - process.SP) * 5;
			}
			else if(strcmp(process.policy, "FIFO") == 0) process.time_slice = process.expected_time;
			process.remain_time = process.expected_time; //remain_time = expected_time at the start
			process.accu_time_slice = 0; //Process hasn't been ran yet, so no accu_time_slice
			process.DP = process.SP; //DP for all processes same as SP initially, but changes for NORMAL processes later	
			process.sleep_avg = 0; //sleep_avg is initially 0
			process_count++;
			
			//Put process into specific queue based on affinity and policy
			if (process.affinity == -1){
				//go through each set of consumer queues and place process in first available spot (while following 1:1:3 rule)
				for(i = 0; i < NUM_THREADS; i++){
					if(strcmp(process.policy, "NORMAL") == 0 && all_queues[i].num_NORMAL < 3){
						if(process.SP < 130){
							pthread_mutex_lock(&lock[i][1]);
							num_processes = all_queues[i].RQ[1].num_processes;
							all_queues[i].RQ[1].process[num_processes] = process;
							all_queues[i].RQ[1].num_processes++;
							pthread_mutex_unlock(&lock[i][1]);
						}
						else{
							pthread_mutex_lock(&lock[i][2]);
							num_processes = all_queues[i].RQ[2].num_processes;
							all_queues[i].RQ[2].process[num_processes] = process;
							all_queues[i].RQ[2].num_processes++;
							pthread_mutex_unlock(&lock[i][2]);
						}
						all_queues[i].num_NORMAL++;
						all_queues[i].total_processes++;
						break;
					}
					else if(strcmp(process.policy, "RR") == 0 && all_queues[i].num_RR == 0){
						pthread_mutex_lock(&lock[i][0]);
						num_processes = all_queues[i].RQ[0].num_processes;
						all_queues[i].RQ[0].process[num_processes] = process;
						all_queues[i].RQ[0].num_processes++;
						all_queues[i].num_RR++;
						all_queues[i].total_processes++;
						pthread_mutex_unlock(&lock[i][0]);
						break;
					}
					else if(strcmp(process.policy, "FIFO") == 0 && all_queues[i].num_FIFO == 0){
						pthread_mutex_lock(&lock[i][0]);
						num_processes = all_queues[i].RQ[0].num_processes;
						all_queues[i].RQ[0].process[num_processes] = process;
						all_queues[i].RQ[0].num_processes++;
						all_queues[i].num_FIFO++;
						all_queues[i].total_processes++;
						pthread_mutex_unlock(&lock[i][0]);
						break;
					}
				}
			}
			else{
				i = process.affinity;
				x = 0; //placeholder to reduce code duplication
				if(strcmp(process.policy, "NORMAL") == 0){
					if(process.SP < 130) x = 1;
					else x = 2;
					all_queues[i].num_NORMAL++;
				}
				else if(strcmp(process.policy, "RR") == 0) all_queues[i].num_RR++;
				else if(strcmp(process.policy, "FIFO") == 0) all_queues[i].num_FIFO++;

				pthread_mutex_lock(&lock[i][x]);
				num_processes = all_queues[i].RQ[x].num_processes;
				all_queues[i].RQ[x].process[num_processes] = process;
				all_queues[i].RQ[x].num_processes++;
				pthread_mutex_unlock(&lock[i][x]);
				all_queues[i].total_processes++;
			}
			all_queues[i].RQ[x] = sort_RQ(all_queues[i].RQ[x]); //Sort process
			//After 5 processes, sleep between 100-600ms and sleep again after each following process
			if(process_count >= 5){
				sleep_time = random_num(100, 500); 
				usleep(sleep_time*1000);	
			}
		}
		printf("[PRODUCER] All processes placed and sorted into respective consumer queues.\n");

	}
	else{
		printf("Could not open input file. Aborting program...\n");
		exit(EXIT_FAILURE);
	}
	printf("[PRODUCER] Producer thread finished.\n");
	thread_finished++;
	pthread_exit(NULL);
}

void *consumer_function(void *arg){
	int i, x;
	int current_RQ; //Variable to track which queue the consumer is checking
	int previous_DP;
	int run_time; //For NORMAL processes, the randomly generated execution time
	int blocked; //Flag for NORMAL processes to set if run_time < time_slice
	struct process_struct process;
	int consumer_number = *(int *)arg;
	printf("[CONSUMER] Consumer %d thread started. Running processes in queues...\n", consumer_number);

	while(all_queues[consumer_number].total_processes > 0){
		if(all_queues[consumer_number].current_RQ > 2) all_queues[consumer_number].current_RQ = 0; //loop back to RQ0
		current_RQ = all_queues[consumer_number].current_RQ;
		pthread_mutex_lock(&lock[consumer_number][current_RQ]);
		for(i = 0; i < all_queues[consumer_number].RQ[current_RQ].num_processes; i++){
			process = all_queues[consumer_number].RQ[current_RQ].process[i];
			//If process was run earlier (just got unblocked)
			if(process.remain_time < process.expected_time && strcmp(process.policy, "NORMAL") == 0){
				gettimeofday(&process.time_at_run, NULL);
				// Divide by 1000 to go from us -> ms. Divide by 100 to go to "ticks" (from lab manual)
				process.sleep_avg += (((process.time_at_run.tv_usec) - (process.time_at_block.tv_usec))/1000) / 100;
				if(process.sleep_avg > 10) process.sleep_avg = 10;
			}

			if(strcmp(process.policy, "NORMAL") == 0){
				run_time = random_num(1, 20)*10; //Generate random number then multiply by 10ms
				process.sleep_avg -= run_time / 100;
				if(process.sleep_avg < 0) process.sleep_avg = 0;
				previous_DP = process.DP;
				process.DP = MAX(100, MIN(previous_DP - process.sleep_avg + 5, 139)); //Calculate new DP
				if(run_time < process.time_slice) blocked = 1;
				else run_time = process.time_slice;
			}
			else{ //Works the same for FIFO and RR
				run_time = process.time_slice;
				previous_DP = process.DP;
			}

			if(process.remain_time - run_time < 0) run_time = process.remain_time;
			
			usleep(run_time*1000); //"run" process
			process.remain_time -= run_time;
			process.accu_time_slice += run_time;
			
			if(process.remain_time == 0){ //remove process from queue if it is has ran for its whole time
				all_queues[consumer_number].total_processes--;
				all_queues[consumer_number].RQ[current_RQ] = remove_process(all_queues[consumer_number].RQ[current_RQ], i);
			}
			else{
				int num_processes; //temp variable
				if(previous_DP < 130 && process.DP >= 130){//Process moved from RQ1 -> RQ2
					//Add process to RQ2
					num_processes = all_queues[consumer_number].RQ[2].num_processes;
					all_queues[consumer_number].RQ[2].process[num_processes] = process;
					all_queues[consumer_number].RQ[2].num_processes++;
					//Remove process from RQ1
					all_queues[consumer_number].RQ[1] = remove_process(all_queues[consumer_number].RQ[1], i);
				}
				else if(previous_DP >= 130 && process.DP < 130){ //Process moved from RQ2 -> RQ1
					//Add process to RQ1
					num_processes = all_queues[consumer_number].RQ[1].num_processes;
					all_queues[consumer_number].RQ[1].process[num_processes] = process;
					all_queues[consumer_number].RQ[1].num_processes++;
					//Remove process from RQ2
					all_queues[consumer_number].RQ[2] = remove_process(all_queues[consumer_number].RQ[2], i);
				}
				else all_queues[consumer_number].RQ[current_RQ].process[i] = process;
			}
			//run_time = service_time from manual, accu_time_slice = total run time so far
			printf("Consumer %d 'ran' process from RQ%d: PID:%d, PrevDP:%d, NewDP:%d, policy:%s, remain_time:%d, time_slice:%d, accu_time_slice:%d, run_time:%d\n",
				consumer_number, current_RQ, process.pid, previous_DP, process.DP, process.policy,
				process.remain_time, process.time_slice, process.accu_time_slice, run_time);
				
			//Wait time for the proces
			if(blocked) usleep(100*1000);
			gettimeofday(&process.time_at_block, NULL);
		}
		//Sort RQ1 and RQ2
		all_queues[consumer_number].RQ[1] = sort_RQ(all_queues[consumer_number].RQ[1]);
		all_queues[consumer_number].RQ[2] = sort_RQ(all_queues[consumer_number].RQ[2]);
		pthread_mutex_unlock(&lock[consumer_number][current_RQ]);
		all_queues[consumer_number].current_RQ++;
	}
	printf("[CONSUMER] Consumer %d finished. Processes in queues: %d\n", consumer_number, all_queues[consumer_number].total_processes);
	thread_finished++;
	pthread_exit(NULL);
}

