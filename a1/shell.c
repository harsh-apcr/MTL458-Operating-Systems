#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_PATH_SIZE 256
#define MAX_CMD_SIZE 130
#define MAX_ARG_SIZE 130
#define MAX_NUM_ARG 50
#define MAX_PROCESSES 2000
#define MAX_PID_DIGITS 6
#define read_command(buf,size,file) {fgets(buf, size, file);}

const char nl = '\n';
const char ws = ' ';

/*
signal handler for our shell
*/


/*
tokenize the input cmd stream into left_args and right_args, also set is_bckgrnd and is_setenv

left_args is an array of strings of size MAX_ARG_SIZE
*/
void tokenize(const char *cmd, char **left_args, char **right_args, bool *is_bckgrnd, bool *is_setenv) {
	char identifier[MAX_ARG_SIZE];
	if (cmd[0] == '&') {
		*is_bckgrnd = true;
		cmd++;
	}
	int i;
	int j = 0;
	left_args[j] = (char*) malloc(MAX_ARG_SIZE * sizeof(char));
	for(i = 0;cmd[i] != 0;i++) {
		if (cmd[i] == '=') {
			i++;
			break;
		}
		if (cmd[i] == ' ') {
			if (cmd[i+1] == '|') {
				i += 3;
				break;
			} else {
				j++;
				left_args[j] = (char*) malloc(MAX_ARG_SIZE * sizeof(char));
				continue;
			}
			
		}
		// cmd[i] != '|' && cmd[i] != '=' && cmd[i] != ' '
		if (cmd[i] == '$') {
			// referencing an identifier
			int k = 0;
			i++;
			while (cmd[i] != ' ' && cmd[i] != 0) {
				identifier[k] = cmd[i];
				i++;k++;
			}
			// cmd[i] == ' ' || cmd[i] == 0
			identifier[k] = 0;
			strcpy(left_args[j], getenv(identifier));
			continue;
		}
		strncat(left_args[j], &cmd[i], 1);
//		printf("%d\t%s\n", j, left_args[j]); 	// DEBUG: debug-line
	}
	if (cmd[i] != 0) {
		// cmd[i-2] == '|' || cmd[i-1] == '='
		// we are assuming that the input follows the grammar specified in the assignment
		if (cmd[i-1] == '=') {
			*is_setenv = true;
			right_args[0] = (char*) malloc(MAX_ARG_SIZE * sizeof(char));
			while(cmd[i] != 0) {
				strncat(right_args[j], &cmd[i], 1); // j == 0
				i++;
			}
//			printf("%d\t%s\n", 0, right_args[0]); // DEBUG : debug-line
		} else if (cmd[i-2] == '|') {
			j = 0;
			right_args[0] = (char*) malloc(MAX_ARG_SIZE * sizeof(char));
			while(cmd[i] != 0) {
				if (cmd[i] == ' ') {
					j++;
					i++;
					right_args[j] = (char*) malloc(MAX_ARG_SIZE * sizeof(char));
					continue;
				}
				strncat(right_args[j], &cmd[i], 1);
//				printf("%d\t%s\n", j, right_args[j]); // DEBUG : debug-line
				i++;
			}

		}
	}
}




void sigint_handler(int signum) {
	if (signum == SIGINT) {
		write(1, &nl, 1);
		exit(0);
	}
}

int main() {

	
	char cwd[MAX_PATH_SIZE];
	char cmd[MAX_CMD_SIZE];
	char *left_args[MAX_ARG_SIZE];
	char *right_args[MAX_ARG_SIZE];
	char cmd_history[5][MAX_CMD_SIZE];
	pid_t process_list[MAX_PROCESSES];
	int process_status_list[MAX_PROCESSES];
	int curr_process = -1;
	
	signal(SIGINT, sigint_handler);
	
	cmd_history[0][0]=0;cmd_history[1][0]=0;cmd_history[2][0]=0;cmd_history[3][0]=0;cmd_history[4][0]=0;
	
	while (1) {
		int i;
		for(i=0;i < MAX_ARG_SIZE;i++) left_args[i] = 0;
		for(i=0;i < MAX_ARG_SIZE;i++) right_args[i] = 0;
		getcwd(cwd, MAX_PATH_SIZE);
		fprintf(stdout, "%s~$ ", cwd);
		read_command(cmd, MAX_CMD_SIZE, stdin);
		cmd[strlen(cmd)-1] = 0;
		for(i=4;i >= 1;i--) strcpy(cmd_history[i], cmd_history[i-1]);
		strcpy(cmd_history[0], cmd);
		
		// tokenize the input command into left_args and right_args
		if (!strcmp(cmd, "exit")) exit(0);
		int status, ret;
		for(i = 0;i <= curr_process;i++) {
			ret = waitpid(process_list[i], &status, WNOHANG);
			if (ret != 0) process_status_list[i] = 1;	// stopped
			else process_status_list[i] = 0;	// running
		}
		
		
		bool is_background = false;
		bool is_setenv = false;
		tokenize(cmd, left_args, right_args, &is_background, &is_setenv);
		
		if (is_background) {
			int pid = fork();
			// TODO : execute background command
			if (pid < 0) {
				fprintf(stderr, "%s command couldn't execute as fork() system call failed\n", left_args[0]);
			} else if (pid == 0) {
				// execute background process
				if (!strcmp(left_args[0], "ps_history")) {
					char process_id[MAX_PID_DIGITS];
					for(i = 0;i <= curr_process;i++) {
						if (process_status_list[i]) {
							sprintf(process_id, "%d", process_list[i]);
							write(1, process_id, strlen(process_id));
							write(1, &ws, sizeof(char));
							write(1, "STOPPED\n", 8 * sizeof(char));
						}
						if (!process_status_list[i]) {
							sprintf(process_id, "%d", process_list[i]);
							write(1, process_id, strlen(process_id));
							write(1, &ws, sizeof(char));
							write(1, "RUNNING\n", 8 * sizeof(char));
						}
					}
					exit(0);
				}
				if (!strcmp(left_args[0], "cmd_history")) {
					for(i = 0;cmd_history[i][0] != 0 && i < 5;i++) {
						if (i > 0 || strcmp(cmd_history[i], "cmd_history")) {
							write(1, cmd_history[i], strlen(cmd_history[i]));
							write(1, &nl, 1);
						}
					}
					exit(0);
				}
				if (execvp(left_args[0], left_args) < 0) {
					fprintf(stderr, "%s command couldn't execute as exec() system call failed\n", left_args[0]);
					exit(1);
				}
			} else {
				process_list[++curr_process] = pid;
				signal(SIGCHLD, SIG_IGN);
			}
		} else if (is_setenv) {
			// TODO : set env variable
			setenv(left_args[0], right_args[0], 1);
		} else if (right_args[0] == 0) {
			// TODO : execute normal command
			int pid = fork();
			if (pid < 0) {
				fprintf(stderr, "%s command couldn't execute as fork() system call failed\n", left_args[0]);
			} else if (pid == 0) {
				if (!strcmp(left_args[0], "ps_history")) {
						char process_id[MAX_PID_DIGITS];
						for(i = 0;i <= curr_process;i++) {
							if (process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "STOPPED\n", 8 * sizeof(char));
							}
							if (!process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "RUNNING\n", 8 * sizeof(char));
							}
						}
						exit(0);
					}
				if (!strcmp(left_args[0], "cmd_history")) {
					for(i = 0;cmd_history[i][0] != 0 && i < 5;i++) {
						if (i > 0 || strcmp(cmd_history[i], "cmd_history")) {
							write(1, cmd_history[i], strlen(cmd_history[i]));
							write(1, &nl, 1);
						}
					}
					exit(0);
				}
				if (execvp(left_args[0], left_args) < 0) {
					fprintf(stderr, "%s command couldn't execute as exec() system call failed\n", left_args[0]);
					exit(1);
				}
			} else {
				process_list[++curr_process] = pid;
				int status;
				waitpid(pid, &status, 0);
			}
		} else {
			// TODO : execute piped command
			int pipefd[2];
			if (pipe(pipefd) < 0) { fprintf(stderr, "could not create pipe\n");continue;};
			pid_t pid1 = fork();
			if (pid1 < 0) {
				fprintf(stderr, "%s command couldn't execute as fork() system call failed\n", left_args[0]);continue;
			} else if (pid1 == 0) {
				close(pipefd[0]);	// pid1 is not going to read from the pipe
				close(1);
				dup(pipefd[1]);
				close(pipefd[1]);
				if (!strcmp(left_args[0], "ps_history")) {
						char process_id[MAX_PID_DIGITS];
						for(i = 0;i <= curr_process;i++) {
							if (process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "STOPPED\n", 8 * sizeof(char));
							}
							if (!process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "RUNNING\n", 8 * sizeof(char));
							}
						}
						exit(0);
					}
				if (!strcmp(left_args[0], "cmd_history")) {
					for(i = 0;cmd_history[i][0] != 0 && i < 5;i++) {
						if (i > 0 || strcmp(cmd_history[i], "cmd_history")) {
							write(1, cmd_history[i], strlen(cmd_history[i]));
							write(1, &nl, 1);
						}
					}
					exit(0);
				}
				if (execvp(left_args[0], left_args) < 0) {
					fprintf(stderr, "%s command couldn't execute as exec() system call failed\n", left_args[0]);
					exit(1);
				}
			} else {
				process_list[++curr_process] = pid1;
				process_status_list[curr_process] = waitpid(pid1 , &status, WNOHANG) ? 1 : 0;
				pid_t pid2 = fork();
				if (pid2 < 0) {
					fprintf(stderr, "%s command couldn't execute as fork() system call failed\n", right_args[0]);
					kill(pid1, SIGKILL);	// abort the first process as the pipelining failed
				} else if (pid2 == 0) {
					close(pipefd[1]);	// pid2 is not going to write to the pipe
					close(0);
					dup(pipefd[0]);
					close(pipefd[0]);
					if (!strcmp(right_args[0], "ps_history")) {
						char process_id[MAX_PID_DIGITS];
						for(i = 0;i <= curr_process;i++) {
							if (process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "STOPPED\n", 8 * sizeof(char));
							}
							if (!process_status_list[i]) {
								sprintf(process_id, "%d", process_list[i]);
								write(1, process_id, strlen(process_id));
								write(1, &ws, sizeof(char));
								write(1, "RUNNING\n", 8 * sizeof(char));
							}
						}
						// ps_history on right side of the pipe will not give the process status of the left side of the pipe
						// as it could either be in running or stopped state and to check that we need to wait() in the parent for
						// it which we don't want
						exit(0);
					}
					
					if (!strcmp(right_args[0], "cmd_history")) {
						for(i = 0;cmd_history[i][0] != 0 && i < 5;i++) {
							if (i > 0 || strcmp(right_args[0], "cmd_history")) {
								write(1, cmd_history[i], strlen(cmd_history[i]));
								write(1, &nl, 1);
							}
						}
						exit(0);
					}
					if (execvp(right_args[0], right_args) < 0) {
						fprintf(stderr, "%s command couldn't execute as exec() system call failed\n", right_args[0]);
						kill(pid1, SIGKILL);
						exit(1);
					}
				} else {
					process_list[++curr_process] = pid2;
					close(pipefd[0]);
					close(pipefd[1]);
					int status;
					// parent process waits untill all child finishes their execution
					waitpid(pid1, &status, 0);waitpid(pid2, &status, 0);	// wait for both the processes to finish
				}
			}
		}
	}
	
}


