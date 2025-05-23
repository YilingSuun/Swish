#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // Print the shell's current working directory
            char dir[CMD_LEN];
            if (getcwd(dir, sizeof(dir)) == NULL) {
                perror("getcwd");
            } else {
                printf("%s\n", dir);
            }
        }

        else if (strcmp(first_token, "cd") == 0) {
            char *dir = strvec_get(&tokens, 1);
            if (dir == NULL) {           // no second argument is provided
                dir = getenv("HOME");    // change to the home directory by default
                if (dir == NULL) {
                    perror("getenv");
                }
            }
            // second argument is provided
            if (chdir(dir) == -1) {
                perror("chdir");
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else {
            // Determine if the last token is "&"
            char *last_tok = strvec_get(&tokens, tokens.length - 1);
            if (strcmp(last_tok, "&") == 0) {
                // If the last token input by the user is "&", start the current
                // command in the background.
                strvec_take(&tokens, tokens.length - 1);
                pid_t child_pid = fork();
                if (child_pid < 0) {
                    printf("error in fork!");
                } else if (child_pid == 0) {
                    if (run_command(&tokens) == -1) {
                        // terminate the child process
                        return 1;
                    }
                } else {
                    // Add a new entry to the jobs list
                    job_list_add(&jobs, child_pid, first_token, BACKGROUND);
                }

            } else {    // start the current command in the foreground
                pid_t child_pid = fork();
                if (child_pid < 0) {
                    printf("error in fork!");
                } else if (child_pid == 0) {
                    if (run_command(&tokens) == -1) {
                        return 1;
                    }
                } else {    // parent process
                    // put the child’s process group in the foreground. This set the child
                    // process as the target of signals sent to the terminal via the keyboard.
                    if (tcsetpgrp(STDIN_FILENO, child_pid) != 0) {
                        perror("tcsetpgrp");
                        strvec_clear(&tokens);
                        printf("%s", PROMPT);
                        continue;
                    }

                    int status;
                    if (waitpid(child_pid, &status, WUNTRACED) == -1) {
                        perror("waitpid");
                        strvec_clear(&tokens);
                        printf("%s", PROMPT);
                        continue;
                    }
                    if (WIFSTOPPED(status)) {    // If stopped, add to job lists as stopped
                        job_list_add(&jobs, child_pid, first_token, STOPPED);
                    }

                    // restore the shell process to the foreground
                    if (tcsetpgrp(STDIN_FILENO, getpid()) != 0) {
                        perror("tcsetpgrp");
                    }
                }
            }
        }

        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }

    job_list_free(&jobs);
    return 0;
}
