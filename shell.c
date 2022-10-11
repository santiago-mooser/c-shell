// file: signal_async_int_default.c
// description: signal handler example for INTERUPT signal
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Signal handler. Just outputs a line to standard error. */
void catcher(int signum){
    switch (signum) {
    case SIGINT:
        printf("\n$$ 3230shell ## ");
        fflush(stdout);
        return;
    default:
        printf("Caught a signal.\n");
        fflush(stdout);
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


/* parse the input line to construct the command name(s) and the associated argument list(s) */
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


int execute(int argc, char *argv[]){

    /* if the command is exit, then exit the shell */
    if (strcmp(argv[0], "exit") == 0){
        exit(0);

    /* If the command is cd, change working directory*/
    } else if (strcmp(argv[0], "cd") == 0){
        if (argc == 1){
            chdir(getenv("HOME"));
        } else {
            chdir(argv[1]);
        }
        return 0;
    }

    /* Initialize pipe */
    int     fd[2], nbytes;
    char    readbuffer[10000] = {};
    enum {WR, RD};
    if (pipe(fd) < 0)
        perror("pipe error");

    /* use fork to create a child process */
    pid_t pid;
    if((pid = fork()) == -1) {
        perror("fork");
        return -1;

    } else if (pid == 0){
       /* If successful, close the read side of the pipe and execute the command with the arguments */
        close(fd[WR]);
        dup2(fd[RD], STDOUT_FILENO);
        int status = execvp(argv[0], argv);
        /* If something goes wrong, exit with an error */
        _exit(status);
    } else {
        /* close output side of pipe in parent process */
        close(fd[RD]);

        /* Get exit status of child*/
        int status;
        waitpid(pid, &status, 0);

        /* If the status of the child is not zero, print the error message */
        if (WEXITSTATUS(status) >= 0 && WEXITSTATUS(status) <= 127){
            /* If the status of the child is zero, read the output of the command and print it */
            nbytes = read(fd[WR], readbuffer, sizeof(readbuffer));
            printf("%s", readbuffer);
            return 0;
        } else if (WEXITSTATUS(status) == 255) {
            if (access(argv[0], F_OK) == 0) {
                // file exists
                printf("'%s': Permission denied\n", argv[0]);
                return 127;
            } else {
                // file doesn't exist
                printf("'%s': No such file or directory\n", argv[0]);
                return 127;
            }
        } else {
            printf("'%s': Command not found\n", argv[0]);
            return 127;
        }
    }
}


// get command line arguments
int main(int argc, char *argv[]) {

    // Register signal handler
    if (install_catcher(SIGINT)) {
        fprintf(stderr, "Cannot install signal handler: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Declare variables */
    char   *line = NULL;
    size_t  size = 0;
    ssize_t len;
        // print prompt


    while (1) {
        printf ( "$$ 3230shell ## " );
        fflush(stdout);
        fflush(stdin);

        /* Reset file variables */
        line = NULL;
        size = 0;
        len = -1;

        /* Read a line from stdin, ignoring EINTR. */
        len = getline(&line, &size, stdin);

        /* If the length is less than zero, there was an error reading the line */
        if (len < 0){
            break;
        }

        /* Clean string by removing newlines, return carriages and spaces */
        while (len > 0 && line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '){
            line[len - 1] = '\0';
            len--;
        }


        /* If the length is zero, just continue. This is to prevent execution when only a newline character is added. */
        if ( len == 0 ){
            continue;
        }

        /* parse command line */
        char *args[1024] = {};
        int num_of_args = 0;
        num_of_args = parse(line, args);

        int result = execute(num_of_args, args);
        if (result <0){
            printf("Error executing command");
        }
        //print out args
        // int d = 0;
        // while (d < num_of_args){
        //     printf("%s \n", args[d]);
        //     fflush(stdout);
        //     d++;
        // }
    }

    exit;
}
