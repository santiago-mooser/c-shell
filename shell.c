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
int parse( char* shell_input, char *args[5][1024] ){

    /* Iterate over the shell input and separate string based on spaces */
    char *token;
    char *token_inner;
    token = strtok(shell_input, "|");


    int i = 0;
    int j = 0;
    char *line;
    while (token != NULL){
        j=0;
        args[i][j] = token;
        token_inner = strtok(args[i][j], " ");
        while (token_inner != NULL){
            args[i][j] = token_inner;
            token_inner = strtok(NULL, " ");
            j++;
        }
        token = strtok(NULL, "|");
        i++;
    }

    return i;
}


int execute(int argc, char *argv[][1024]){


    /* if the command is exit, then exit the shell */
    if (strcmp(argv[0][0], "exit") == 0){
        exit(0);

    /* If the command is cd, change working directory*/
    } else if (strcmp(argv[0][0], "cd") == 0){
        if (strlen(argv[0][0]) == 1){
            chdir(getenv("HOME"));
        } else {
            chdir(argv[0][1]);
        }
        return 0;
    }


    /* For every argv line, create a pipe and connect pipe. For the last pipe, connect it to the current program. */
    int pipes[argc][2];
    enum {RD, WR};
    pid_t pids[argc];

    /* Finally, prepare some buffer for the parent program */
    int nbytes;
    char readbuffer[10000] = {};

    /* Create the pipe the processes are going to output to */
    for (int z=0; z < argc; z++){
        if (pipe(pipes[z]) < 0){
            perror("pipe error");
        }
    }

    int i;
    for (i = 0; i < argc; i++){

        /* Fork the process */
        if((pids[i] = fork()) == -1) {
            perror("fork");
            return -1;
        }
        if (pids[i] == 0){
            /* Child process */

            if (i != 0){
                /* If the fork is not the first process, set the previous process' pipe as the STDIN */
                if(dup2(pipes[i-1][RD], STDIN_FILENO) < 0){
                    printf("Unable to set STIN\n");
                    exit(1);
                }

            }

            /* Set the newly created pipe as the STDOUT for the forked process */
            if(dup2(pipes[i][WR], STDOUT_FILENO) < 0){
                printf("Unable to set STDOUT\n");
                exit(1);
            }
            /* Close the pipes */
            for (int z=0; z < argc; z++){
                close(pipes[z][RD]);
                close(pipes[z][WR]);
            }

            /* Finally, simply execute the program */
            execvp(argv[i][0], argv[i]);
            /* Exit with an error if the program is unable to be executed */
            exit(127);
        }
    }

    /* Once the child processes have been created, close all pipes except the last one */
    for (int z=0; z < argc-1; z++){
        close(pipes[z][RD]);
        close(pipes[z][WR]);
    }

    /* Then also close the write end of the last pipe */
    close(pipes[argc-1][WR]);

    int status;
    /* Then, we obviously need to wait for the last process to finish */
    waitpid(pids[argc-1], &status, 0);

    /* And read the buffer */
    nbytes = read(pipes[argc-1][RD], readbuffer, sizeof(readbuffer));
    printf("%s", readbuffer);
    fflush(stdout);

    /* Now we can check the exit status of the executed processes */

    /* If the status of the child is not an error, exit with that code*/
    if (WEXITSTATUS(status) >= 0 && WEXITSTATUS(status) <= 127){
        return WEXITSTATUS(status);

    /* Otherwise print message to screen and exit */
    } else if (WEXITSTATUS(status) == 255) {
        if (access(argv[i][0], F_OK) == 0) {
            // file exists
            printf("'%s': Permission denied\n", argv[i][0]);
            return 127;
        } else {
            // file doesn't exist
            printf("'%s': No such file or directory\n", argv[i][0]);
            return 127;
        }
    } else {
        printf("'%s': Command not found\n", argv[i][0]);
        return 127;
    }

}


// get command line arguments
int main(int argc, char *argv[]) {

    // Register signal handler
    if (install_catcher(SIGINT)) {
        fprintf(stderr, "Cannot install signal handler: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    while (1) {

        /* Declare/reinitialize variables so we can reuse them */
        char    *line = NULL;
        char    *input = NULL;
        size_t  buffer_size = 32;
        ssize_t char_len, char_len_input;

        /* Print the prompt */
        printf ( "$$ 3230shell ## " );
        fflush(stdout);

        /* Read a line from stdin, ignoring EINTR. */
        char_len = getline(&line, &buffer_size, stdin);

        /* If the length is less than zero, there was an error reading the line. */
        /* Respond accordingly */
        if (char_len < 0){
            if (errno == EINTR){
                continue;
            } else {
                perror("getline");
                exit(EXIT_FAILURE);
            }
        }

        /* Clean string by removing newlines, return carriages and spaces */
        while (char_len > 0 && line[char_len - 1] == '\n' || line[char_len - 1] == '\r' || line[char_len - 1] == ' '){
            line[char_len - 1] = '\0';
            char_len--;
        }
        /* If the last character is a '\' then continue collecting input and concatenate it to the input string after removing the '\' character */
        while (line[char_len - 1] == '\\'){
            line[char_len - 1] = '\0';
            char_len--;
            char_len_input = getline(&input, &buffer_size, stdin);
            while (char_len > 0 && input[char_len_input - 1] == '\n' || input[char_len_input - 1] == '\r' || input[char_len_input - 1] == ' '){
                input[char_len_input - 1] = '\0';
                char_len_input--;
            }
            line = strcat(line, input);
        }

        while (char_len > 0 && line[char_len - 1] == '\n' || line[char_len - 1] == '\r' || line[char_len - 1] == ' '){
            line[char_len - 1] = '\0';
            char_len--;
        }

        /* If the length is zero, just continue. This is to prevent execution when only a newline character is added. */
        if ( char_len == 0 ){
            continue;
        }

        /* parse command line */
        char *args[5][1024] = {};
        int num_of_args = 0;
        num_of_args = parse(line, args);
        if (num_of_args == -1){
            continue;
        }
        execute(num_of_args, args);

        fflush(stdin);
        /* Reset file variables */
        free(line);
        free(input);
        buffer_size    = 0;
        char_len     = -1;
    }
}
