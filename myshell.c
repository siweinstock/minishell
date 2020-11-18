#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define STD 0       // default command (no pipeline or ampersand)
#define BG 1        // background command (ampersand)
#define PIPE 2      // piped command (pipeline)

int prepare(void) {
    return 0;
}

int finalize(void) {
    return 0;
}


void handler() {

}

void disable_sigint() {
}


int exec_cmd(char** arglist, int mode) {
    int pid = fork();

    if (pid == -1) {
        perror("Failed forking\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) { //parent
        if (mode == 0) {
            wait(NULL);
        }
    }
    else { // child
        execvp(arglist[0], arglist);
    }

    return 0;
}


int exec_pipe_2(int count, char** arglist, int i) {
    int fd[2];
    pipe(fd);

    int pid1 = fork();

    if (pid1 == 0) {
        //child
        dup2(fd[1], STDOUT_FILENO); // replace stdout with output side of pipeline
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[0], arglist);
        printf("ERROR\n");
    }
    else {
        //parent
        close(fd[1]);

        int pid2 = fork();

        if (pid2 == 0) {
            // 2nd child
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execvp(arglist[i+1], arglist+i+1);
            printf("ERROR\n");
        }
        else {
            close(fd[0]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }

    }
    return 0;
}


int exec_pipe(int count, char** arglist, int i) {
    int fd[2]; // fd[0] - read ; fd[1] - write
    pipe(fd);

    int id1 = fork();

    if (id1 == 0) { // child (ls -l)
        dup2(fd[1], STDOUT_FILENO); // redirect stdout to pipe
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[0], arglist); // arglist until first NULL encountered
    }
    // only parent executes code from here on
    close(fd[1]);

    int id2 = fork();

    if (id2 == 0) { // child
        dup2(fd[0], STDIN_FILENO); // redirect stdin to pipe
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[i+1], arglist+i+1);
    }

    close(fd[0]);


    waitpid(id1, NULL, 0);
    waitpid(id2, NULL, 0);

    return 0;

}


/*
 * search for pipeline in command.
 * if found - return pipeline's index in arglist
 * if not found - return -1
 *
 * */
int is_piped(int count, char** arglist) {
    int i;

    for (i=0; i<count; i++) {
        if (*arglist[i] == '|') {
            return i;
        }
    }

    return -1;
}

int process_arglist(int count, char** arglist) {
    int exec_mode = STD;
    int i;
    int pipeline = -1;

    for (i=count-1; i>0; i--) {
      if (*arglist[i] == '&') {
          exec_mode = BG;
          break;
      }
      else if (*arglist[i] == '|') {
          exec_mode = PIPE;
          pipeline = i;
          break;
      }
    }

    switch (exec_mode) {
        case PIPE:
            arglist[pipeline] = NULL;
            exec_pipe(count, arglist, pipeline);
            break;
        case BG:
            arglist[count-1] = NULL;
        default:
            exec_cmd(arglist, exec_mode);
            break;
    }

/*
    if (*arglist[count-1] == '&') { // bg command
        arglist[count-1] = NULL;
        mode = BG;
    }
    else if (is_piped(count, arglist) != -1) { // command contains pipeline
        mode = PIPE;
    }
*/
    //
    return 1;
}
