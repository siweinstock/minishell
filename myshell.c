#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define STD 0       // default command (no pipeline or ampersand)
#define BG 1        // background command (ampersand)
#define PIPE 2      // piped command (pipeline)

#define ENABLE 1
#define DISABLE 0

/*
 * enable/disable process responce to the SIGINT (CTRL-C) signal
 * status gets values:
 * - ENABLE: set SIGINT default behavior
 * - DISABLE: ignore SIGINT
 *
 * */
int toggle_sigint(int status) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    if (status == ENABLE) {
        sa.sa_handler = SIG_DFL;
    }
    else if (status == DISABLE) {
        sa.sa_handler = SIG_IGN;
    }
    sa.sa_flags = SA_RESTART;

    return sigaction(SIGINT, &sa, NULL);
}

// shell initialization
int prepare(void) {
    toggle_sigint(DISABLE);
    return 0;
}

// shell cleanup
int finalize(void) {
    return 0;
}



/*
 * Execute a command from shell prompt.
 * arglist - parsed command into array of strings.
 *           Last element should always be NULL
 * mode - execution mode:
 *        STD - foreground command
 *        BG  - background command
 *
 * */
int exec_cmd(char** arglist, int mode) {
    int pid = fork();

    if (pid == -1) {
        perror("Failed forking\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) { //parent
        if (mode == STD) {
            //wait(NULL);
            waitpid(pid, NULL, 0);
        }
        else {  // background command
            waitpid(pid, NULL, WNOHANG);
        }
    }
    else { // child
        toggle_sigint(ENABLE);
        execvp(arglist[0], arglist);
    }

    return 0;
}

/*
 * Execute a command that contains a pipeline (PIPE cmd type)
 * arglist - parsed command into array of strings.
 *           Last element should always be NULL
 *           Pipeline index is also NULL
 * pipe_index - index of pipeline in arglist (which is now NULL)
 *
 * */
int exec_pipe(char** arglist, int pipe_index) {
    int fd[2]; // fd[0] - read ; fd[1] - write
    pipe(fd);

    int id1 = fork();

    if (id1 == 0) { // child
        toggle_sigint(ENABLE);
        dup2(fd[1], STDOUT_FILENO); // redirect stdout to pipe
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[0], arglist); // arglist until first NULL encountered
    }

    // only parent executes code from here on
    close(fd[1]);

    int id2 = fork();

    if (id2 == 0) { // child
        toggle_sigint(ENABLE);
        dup2(fd[0], STDIN_FILENO); // redirect stdin to pipe
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[pipe_index + 1], arglist + pipe_index + 1);
    }

    close(fd[0]);

    waitpid(id1, NULL, WNOHANG);
    waitpid(id2, NULL, WNOHANG);

    return 0;

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
            exec_pipe(arglist, pipeline);
            break;
        case BG:
            arglist[count-1] = NULL;
        default:
            exec_cmd(arglist, exec_mode);
            break;
    }

    return 1;
}
