#define _GNU_SOURCE

#include "swish_funcs.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"

#define MAX_ARGS 10

// Tokenize string s
int tokenize(char *s, strvec_t *tokens) {
    char *str = strtok(s, " ");
    if (str == NULL) {
        perror("Failed to Tokenize");
        return -1;
    }
    while (str != NULL) {
        // Add each token to the 'tokens' parameter (a string vector)
        if (strvec_add(tokens, str) == -1) {
            return -1;
        }
        str = strtok(NULL, " ");
    }
    return 0;
}

// Execute the specified program (token 0) with the
// specified command-line arguments
int run_command(strvec_t *tokens) {
    struct sigaction sac;
    sac.sa_handler = SIG_DFL;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    // Change the process group of this process (a child of the main shell).
    if (setpgid(getpid(), getpid()) != 0) {
        perror("setpgid");
    }
    // perform output redirection
    int index1 = strvec_find(tokens, "<");
    int index2 = strvec_find(tokens, ">");
    int index3 = strvec_find(tokens, ">>");
    if (index1 != -1) {    // check for the presence of strings < in the tockens
        int fd = open(tokens->data[index1 + 1], O_RDONLY);
        if (fd == -1) {
            perror("Failed to open input file");
            return -1;
        }
        // redirect standard input to the opened files
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (index2 != -1) {    // check for the presence of strings > in the tockens
        int fd = open(tokens->data[index2 + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("Failed to open output file");
            return -1;
        }
        // redirect standard output to the opened files
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    } else if (index3 != -1) {    // check for the presence of strings >> in the tockens
        int fd = open(tokens->data[index3 + 1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("Failed to open output file");
            return -1;
        }
        // redirect standard output to the opened files
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    // delete the redirection operators and their operands from program param
    if (index1 != -1) {
        strvec_take(tokens, index1);
    } else if (index2 != -1) {
        strvec_take(tokens, index2);
    } else if (index3 != -1) {
        strvec_take(tokens, index3);
    }

    // Build a string array from the 'tokens' vector with NULL sentinel at the end
    char *str_arr[tokens->length + 1];
    for (int i = 0; i < tokens->length; i++) {
        str_arr[i] = tokens->data[i];
    }
    str_arr[tokens->length] = NULL;

    execvp(str_arr[0], str_arr);
    // If run_command() fails
    perror("exec");
    return -1;
}

// Implement the ability to resume stopped jobs
int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // get the job index supplied by the user (in tokens index 1)
    int index;
    char *str_index = strvec_get(tokens, 1);
    if (sscanf(str_index, "%d", &index) == EOF) {    // convert string into int
        perror("sscanf");
        return -1;
    }
    // look up the relevant job information from the jobs list
    job_t *job = job_list_get(jobs, index);
    if (job == NULL) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }
    if (is_foreground) {
        // move the job’s process group into the foreground
        if (tcsetpgrp(STDIN_FILENO, job->pid) == -1) {
            perror("tcsetpgrp");
            return -1;
        }
        // Send the process the SIGCONT signal with the kill() system call
        if (kill(job->pid, SIGCONT) == -1) {
            perror("kill");
            return -1;
        }

        // wait for the program to terminate or be paused once again
        int status;
        if (waitpid(job->pid, &status, WUNTRACED) == -1) {
            perror("waitpid");
            return -1;
        }
        // If the job has terminated (not stopped), remove it from the 'jobs' list
        if (!WIFSTOPPED(status)) {
            if (job_list_remove(jobs, index) == -1) {
                printf("Fail to remove job from the 'jobs' list\n");
                return -1;
            }
        }
        // restore the shell process to the foreground
        if (tcsetpgrp(STDIN_FILENO, getpid()) != 0) {
            perror("tcsetpgrp");
            return -1;
        }
    } else {    // resume stopped jobs in the background.
        // Send the process the SIGCONT signal with the kill() system call
        if (kill(job->pid, SIGCONT) == -1) {
            perror("kill");
            return -1;
        }
        // modify the 'status' field of the relevant job list entry to BACKGROUND
        job->status = BACKGROUND;
    }
    return 0;
}

// Wait for a specific job to stop or terminate
int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // get the job index supplied by the user (in tokens index 1)
    int index;
    char *str_index = strvec_get(tokens, 1);
    if (sscanf(str_index, "%d", &index) == EOF) {    // convert string into int
        perror("sscanf");
        return -1;
    }
    // Look up the relevant job information  from the jobs list
    job_t *job = job_list_get(jobs, index);
    if (job == NULL) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }
    // Make sure the job's status is BACKGROUND
    if (job->status == STOPPED) {
        fprintf(stderr, "Job index is for stopped process not background process\n");
        return -1;
    }
    // Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    int status;
    if (waitpid(job->pid, &status, WUNTRACED) == -1) {
        perror("waitpid");
        return -1;
    }
    // If the job has terminated, remove it from the 'jobs' list
    if (!WIFSTOPPED(status)) {
        if (job_list_remove(jobs, index) == -1) {
            printf("Fail to remove job from the 'jobs' list\n");
        }
    }
    return 0;
}

// Wait for all background jobs to stop or terminate
int await_all_background_jobs(job_list_t *jobs) {
    // Iterate through the jobs list
    for (int i = 0; i < jobs->length; i++) {
        // For a background job, call waitpid() with WUNTRACED.
        if (job_list_get(jobs, i)->status == BACKGROUND) {
            int status;
            if (waitpid(job_list_get(jobs, i)->pid, &status, WUNTRACED) == -1) {
                perror("waitpid");
                return -1;
            }
            // If the job has stopped, change its status to STOPPED.
            if (WIFSTOPPED(status)) {
                job_list_get(jobs, i)->status = STOPPED;
            }
        }
    }
    // Remove all background jobs (which have all just terminated) from jobs list.
    job_list_remove_by_status(jobs, BACKGROUND);

    return 0;
}
