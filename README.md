# LinuxScheduler

This project simulates a 4-core solution to the simplified linux scheduling policy using threads. There is 1 producer thread, which reads process information from a pre-existing file, and 4 consumer threads, representing 1 core each. The producer puts these processes into ready queues corresponding to core affinity of the process or available consumer queue space. Each consumer has a set of 3 ready queues from which it reads and "runs" the processes. Once all the processes have been run, the program ends.

How to execute the program (on Linux):
	1) chmod +x test
	2) ./test
	3) Once xterm windows will pop up, execute this command:
		./pc_scheduler
