// file: signal_async_int_default.c
// description: signal handler example for INTERUPT signal
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "COMP3230_signal.h"

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2

int cont = 0;

/* Helper function: Write a string to a descriptor, keeping errno unchanged.
   Returns 0 if success, errno error code if an error occurs. */
static inline int  wrs(const int fd, const char *s)
{
    /* Invalid descriptor? */
    if (fd == -1)
        return EBADF;

    /* Anything to actually write? */
    if (s && *s) {
        const int   saved_errno = errno;
        const char *z = s;
        ssize_t     n;
        int         err = 0;

        /* Use a loop to find end of s, since strlen() is not async-signal safe. */
        while (*z)
            z++;

        /* Write the string, ignoring EINTR, and allowing short writes. */
        while (s < z) {
            n = write(fd, s, (size_t)(z - s));
            if (n > 0)
                s += n;
            else
            if (n != -1) {
                /* This should never occur, so it's an I/O error. */
                err = EIO;
                break;
            } else
            if (errno != EINTR) {
                /* An actual error occurred. */
                err = errno;
                break;
            }
        }

        errno = saved_errno;
        return err;

    } else {
        /* Nothing to write. NULL s is silently ignored without error. */
        return 0;
    }
}

/* Signal handler. Just outputs a line to standard error. */
void catcher(int signum)
{
    switch (signum) {
    case SIGINT:
        wrs(STDERR_FILENO, "\n$$ 3230shell ## ");
        return;
    default:
        wrs(STDERR_FILENO, "Caught a signal.\n");
        return;
    }
}

/* Helper function to install the signal handler. */
int install_catcher(const int signum)
{
    struct sigaction  act;

    memset(&act, 0, sizeof act);
    sigemptyset(&act.sa_mask);

    act.sa_handler = catcher;
    act.sa_flags = SA_RESTART;  /* Do not interrupt "slow" functions */
    if (sigaction(signum, &act, NULL) == -1)
        return -1;  /* Failed */

    return 0;
}


// parses the input line to construct the command name(s) and the associated argument list(s)
int parse( char* shell_input, char **args ){

    char *token;
    token = strtok(shell_input, " ");

    int i = 0;
    while (token != NULL){
        args[i] = token;
        token = strtok(NULL, " ");
        i++;
    }

    return i;
}



int execute(int argc, char *args[]){

    char *cmd = args[0];
    // if the command is exit, then exit the shell
    if (strcmp(cmd, "exit") == 0){
        exit(0);
    }

    // if the command is cd, then change the directory
    if (strcmp(cmd, "cd\n") == 0){
        if (argc == 1){
            chdir(getenv("HOME"));
        } else {
            chdir(args[1]);
        }
        return 0;
    }

    // if the command is pwd, then print the current working directory
    if (strcmp(cmd, "pwd\n") == 0){
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    }

    // use fork to create a child process
    pid_t pid = fork();
    //Then execute the command with the arguments
    if (pid == 0){
        execvp(cmd, args);
        exit(0);
    } else {
        wait(NULL);
    }
}


// get command line arguments
int main(int argc, char *argv[]) {

    // Register signal handler
    if (install_catcher(SIGINT)) {
        fprintf(stderr, "Cannot install signal handler: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // check for correct number of arguments
    char   *line = NULL;
    size_t  size = 0;
    ssize_t len;

        // print prompt


    while (1) {
        printf ( "$$ 3230shell ## " );
        len = getline(&line, &size, stdin);
        if (len < 0){
            break;
        }

        if (!strcmp(line, "exit\n") || !strcmp(line, "quit\n")){
            break;
        }
        // parse command line
        char *args[30];
        int num_of_args;
        num_of_args = parse(line, args);

        execute(num_of_args, args);
        //print out args
        // int d = 0;
        // while (d < num_of_args){
        //     printf("%s \n", args[d]);
        //     fflush(stdout);
        //     d++;
        // }
    }

}
