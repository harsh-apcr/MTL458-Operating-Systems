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
#define read_command(buf,size,file) {fgets(buf, size, file);}

const char nl = '\n';

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

// input is parsed correctly
int mmain() {
	const char* cmd = "identifier=value\n";
	char* left_args[MAX_NUM_ARG];
	char* right_args[MAX_NUM_ARG];
	int i;
	for(i=0;i < MAX_NUM_ARG;i++) left_args[i] = 0;
	for(i=0;i < MAX_NUM_ARG;i++) right_args[i] = 0;
	bool is_background = false;
	bool is_setenv = false;
	tokenize(cmd, left_args, right_args, &is_background, &is_setenv);
	for(i = 0;left_args[i] != 0;i++) printf("%d %s ", i, left_args[i]);
	printf("\n");
	for(i = 0;right_args[i] != 0;i++) printf("%d %s ",i , right_args[i]);
	printf("\n");
	for(i=0;left_args[i] != 0;i++) free(left_args[i]);
	for(i=0;right_args[i] != 0;i++) free(right_args[i]);

}



int main() {
	
	char cwd[MAX_PATH_SIZE];
	char cmd[MAX_CMD_SIZE];
	char *left_args[MAX_ARG_SIZE];
	char *right_args[MAX_ARG_SIZE];
	char cmd_history[5][MAX_CMD_SIZE];
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
				wait(0);
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
				pid_t pid2 = fork();
				if (pid2 < 0) {
					fprintf(stderr, "%s command couldn't execute as fork() system call failed\n", right_args[0]);
					kill(pid1, SIGKILL);	// abort the first process as the pipelining failed
				} else if (pid2 == 0) {
					close(pipefd[1]);	// pid2 is not going to write to the pipe
					close(0);
					dup(pipefd[0]);
					close(pipefd[0]);
					if (!strcmp(right_args[0], "cmd_history")) {
						for(i = 0;cmd_history[i][0] != 0 && i < 5;i++) {
							if (i > 0 || strcmp(cmd_history[i], "cmd_history")) {
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
					close(pipefd[0]);
					close(pipefd[1]);
					// parent process waits untill all child finishes their execution
					wait(0);wait(0);	// wait for both the processes to finish
				}
			}
		}
	}
	
}


