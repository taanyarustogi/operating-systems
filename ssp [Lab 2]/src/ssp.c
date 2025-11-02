#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    pid_t pid;
    char name[256];
    int status;
    int ssp_id;
} process;

static process orphans[256]; // max orphan processes
static int numorphans;

static process processes[256]; // max processes
static int numprocess;
static int length;

void handle_signal(int signum) {
    if(signum == SIGCHLD) {
        int stat;
        bool boo; //if processes is an orphan = true
        pid_t pid;
        while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) { 
            //go through all the processes
            boo = true;
            for (int i = 0; i < numprocess ; i++) {
                if(pid == processes[i].pid) {
                    if(WIFEXITED(stat)) {
                        processes[i].status = WEXITSTATUS(stat); //if  ended normally, status = exist status
                    } else if (WIFSIGNALED(stat)) {
                        processes[i].status = WTERMSIG(stat) + 128; //if ended through a signal, status = signal number + 128
                    }
                    boo = false;
                    break;
                }
            }

            if(boo) {
                orphans[numorphans].pid = pid; //add orphan processes to orphan array
                if(9 > length) {
                    length = 9;
                }
                if(WIFEXITED(stat)) {
                    orphans[numorphans].status = WEXITSTATUS(stat); //if  ended normally, status = exist status
                } else if (WIFSIGNALED(stat)) {
                    orphans[numorphans].status = WTERMSIG(stat) + 128; //if ended through a signal, status = signal number + 128
                }
                numorphans++; // number of orphan processes
            }
        }
    }
}

void register_signal(int sigum) { //from lecture
    struct sigaction new_action = {0};
    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = handle_signal;
    new_action.sa_flags = SA_RESTART | SA_NOCLDSTOP; //from lab handout
    if(sigaction(sigum, &new_action, NULL) == -1) {
        int err = errno;
        perror("Error: sigaction");
        exit(err);
    }
}

void ssp_init() {
    numprocess = 0;
    length = 0;
    numorphans = 0;
    prctl(PR_SET_CHILD_SUBREAPER, 1);
    register_signal(SIGCHLD);
    //initalise or setup anything you need
}

int ssp_create(char *const *argv, int fd0, int fd1, int fd2) {
     pid_t child = fork(); //create new process

    if (child < 0) {
        perror("Error: fork");
        exit(errno);
    }

    if (child == 0) { 
        //set file descriptors 0,1,2 to match fd0,fd1,fd2
        if ((dup2(fd0, 0) == -1) || (dup2(fd1, 1) == -1) || (dup2(fd2, 2) == -1)) {
            perror("Error: dup2 with fd0");
            exit(errno);
        } 

        //close all file descriptors except 0,1,2
        DIR *direc = opendir("/proc/self/fd");
        if (direc == NULL) {
            perror("Error: opendir");
            exit(errno);
        }
        struct dirent *openfile;
        while((openfile = readdir(direc)) != NULL) {
            if(openfile->d_type == DT_LNK) { //check if d_type is set to DT_LNK
                int num = atoi(openfile->d_name);
                if (num > 2) {
                    close(num);
                }
            }
        }
        //if execvp fails, exit with errno
        int fail = execvp(argv[0], argv);
        if(fail == -1) {
            perror("Error: execvp");
            exit(errno);
        }
    } else {
        processes[numprocess].pid = child;
        strncpy(processes[numprocess].name, argv[0], sizeof(processes[numprocess].name) - 1); //copy argv[0] and record the name
        if(strlen(argv[0]) > length) {
            length = strlen(argv[0]);
        }
        processes[numprocess].status = -1; //set status to -1 initally
        processes[numprocess].ssp_id = numprocess; //ssp_id starts with 0
        numprocess++;
    }

    return processes[numprocess-1].ssp_id; //return ssp_id
}

int ssp_get_status(int ssp_id) {
    //get status of the process referred to by ssp_id without blocking.
    int stat = processes[ssp_id].status;
    if (stat >= 0) {
        return stat;
    }
    int wait;
    pid_t pid = waitpid(processes[ssp_id].pid, &wait, WNOHANG);
    if(pid == -1) {
        perror("Error: waitpid");
        exit(errno);
    } 
    if(pid == 0) {
        processes[ssp_id].status = -1; //process is still running
    } else if (pid == processes[ssp_id].pid) {
        if(WIFEXITED(wait)) {
            processes[ssp_id].status = WEXITSTATUS(wait); //if  ended normally, status = exist status
        } 
        if (WIFSIGNALED(wait)) {
            processes[ssp_id].status = WTERMSIG(wait) + 128; //if ended through a signal, status = signal number + 128
        }
    }
    return processes[ssp_id].status;

}

void ssp_send_signal(int ssp_id, int signum) {
    //send signal signum to the process referred to by the ssp_id. 
    if (ssp_id < 0 || ssp_id >=  numprocess) {
        return; //invalid ssp_id
    }
    pid_t pid = processes[ssp_id].pid;
    if(kill(pid, signum) == -1) {
        //do nothing if there is an error (process not running)
    }
}

void ssp_wait() {
    for (int i = 0; i < numprocess; i++) {
        int stat;
        if (processes[i].status == -1) {
            pid_t pid = waitpid(processes[i].pid, &stat, 0); //block and only return whan all processes are terminated

            if(pid == -1){
                perror("Error: waitpid");
                exit(errno);
            }

            if(pid == processes[i].pid) {
                if(WIFEXITED(stat)) {
                    processes[i].status = WEXITSTATUS(stat); //if  ended normally, status = exist status
                } else if (WIFSIGNALED(stat)) {
                    processes[i].status = WTERMSIG(stat) + 128; //if ended through a signal, status = signal number + 128
                }
            }
        }
    }
}

void ssp_print() {
    if (length <= 3) {
        printf("    PID CMD STATUS\n"); //header
        //processes
        for(int i=0; i < numprocess; i++) {
            printf("%7d %s %*d\n", processes[i].pid, processes[i].name, 4-length, processes[i].status);
        }
    } else {
        printf("%7s %-*s STATUS\n", "PID", length, "CMD"); //header
        //processes
        for(int i=0; i < numprocess; i++) {
            printf("%7d %--*s %d\n", processes[i].pid, length, processes[i].name, processes[i].status);
        }
        if (numorphans > 0) {
            for (int i = 0; i < numorphans; i++) {
                printf("%7d %--*s %d\n", orphans[i].pid, length, "<unknown>", orphans[i].status);
            }
        }
    }
}
