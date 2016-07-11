/* Sam Beaulieu
 * EC 535, Spring 2016
 * Lab 3, 3/18/16
 * Source code for kernel module mytimer user level interface ktimer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>


/******************************************************

  Usage:
    ./ktimer [flag] [argument1] [message]
	 
	-l (list timers)
	-s (set timers)
	
  Examples:
	./ktimer -l
		print all the timers currently active

	./ktimer -s 10 ThisIsAMessage
		set a timer with the message "ThisIsAMessage" and an expiration time in 10 seconds
		only one timer is supported. Timer can be updated by using the same name.
	
******************************************************/

void printManPage(void);
void sighandler(int);

int main(int argc, char **argv) {
	char line[128];
	int pFile, oflags;
	struct sigaction action, oa;
	
	/* Check to see if the ktimer successfully has mknod run
	   Assumes that ktimer is tied to /dev/mytimer */
	pFile = open("/dev/mytimer", O_RDWR);
	if (pFile < 0) {
		fputs("mytimer module isn't loaded\n",stderr);
		return -1;
	}

	// Setup signal handler
	memset(&action, 0, sizeof(action));
	action.sa_handler = sighandler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGIO, &action, NULL);
	fcntl(pFile, F_SETOWN, getpid());
	oflags = fcntl(pFile, F_GETFL);
	fcntl(pFile, F_SETFL, oflags | FASYNC);

	// Check if in read mode
	if (argc >= 2 && strcmp(argv[1], "-l") == 0) 
	{
		read(pFile, line, 128);
		if(strlen(line) > 0)
			printf("%s\n", line);
	}
	// Check if in write mode
	else if (argc > 3 && strcmp(argv[1], "-s") == 0) 
	{
		// Create string to be written to file
		char pid[16];
		sprintf(pid, "%d", getpid());
		char *temp = malloc(strlen(argv[2]) + strlen(argv[3]) + 16 + 3);
		strcpy(temp, argv[2]);
		strcat(temp, " ");
		strcat(temp, pid);
		strcat(temp, " ");
		strcat(temp, argv[3]);

		// Write information string to file
		write(pFile, temp, strlen(temp));

		// Free memory
		free(temp);

		// Read
		read(pFile, line, 128);
		if(line[0] != '-')
		{
			pause();
			printf("%s\n", argv[3]);
		}
		else
		{
			printf("%s",line+1);
		}
	}
	// Otherwise invalid
	else {
		printManPage();
	}

	close(pFile);
	return 0;
}

// SIGIO handler
void sighandler(int signo)
{
}

void printManPage() {
	printf(" ktimer [-flag] [argument] [message]\n");
	printf(" -l: list [argument] timers from the mytimer module\n");	
	printf(" -s: set a timer with [argument] seconds until expiration,\n\twith [message] label using the mytimer module\n");
	printf("Notes:\nOnly one timer is supported.\n");
	printf("Processess should be run in the background (using & at the end of commands).\n");
	printf("The \"set +m\" command should be used prior to running any ktimer commands.\n");
	printf("\tThis hides the \"done\" output of the background processes.\n");
	printf("If a process is killed, it's message will not be printed but the timer\n");
	printf("\twill not be deleted so it can still be updated/seen with -l or through the proc file.\n");
}
