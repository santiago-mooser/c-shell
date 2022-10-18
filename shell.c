#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

/*
Student name and No.:
    Santiago Espinosa Mooser
    3035557399

Development platform:
    Arch Linux (kernel 5.15.73-3-lts)
    gcc (GCC) 12.2.0

Remarks:
    - The parsing does not allow parameters with quotation marks and a space. This would not work:

        ls -al "folder name"

    - timeX command is not implemented
    - Background command is
*/



struct command {
    char    *argv[5][1024];
    int     timex[5];
    int     argc;
    int     isBackground;
};



/* Signal handler. Just outputs a line to standard error. */
void shell_catcher(int signum){
    switch (signum) {
    case SIGINT:
        printf("\n$$ 3230shell ## ");
        fflush(stdout);
        return;
    default:
        return;
    }
}

void child_catcher(int signum){
    switch (signum) {
    case SIGKILL:
        printf("Killed");
        return;
    case SIGINT:
        printf("Interrupted");
        return;
    default:
        return;
    }
}

/* Helper function to install the signal handler. */
int install_catcher(const int signum, char *type){

    struct sigaction  act;

    memset(&act, 0, sizeof act);
    sigemptyset(&act.sa_mask);

    if (strcmp(type, "shell") == 0) {
        act.sa_handler = shell_catcher;
    } else if (strcmp(type, "child") == 0) {
        act.sa_handler = child_catcher;
    } else {
        return -1;
    }

    act.sa_flags = SA_RESTART;  /* Do not interrupt "slow" functions */
    if (sigaction(signum, &act, NULL) == -1)
        return -1;  /* Failed */

    return 0;
}


/* parse the input line to construct the command name(s) and the associated argument list(s) */
void parse( char* line, struct command* cmd ){

    char    *input = NULL;
    size_t  buffer_size = 32;
    ssize_t char_len, char_len_input;

    /* Read a line from stdin, ignoring EINTR. */
    char_len = getline(&line, &buffer_size, stdin);

    /* If the length is less than zero, there was an error reading the line. */
    /* Respond accordingly */
    if (char_len < 0){
        if (errno == EINTR){
            cmd->argc = -1;
            return;
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
        cmd->argc = -1;
        return;
    }


    /* Iterate over the shell input and separate string based on spaces */
    char *token;
    char *token_inner;
    token = strtok(line, " ");

    int params_index = 0;
    char *temp[1024];
    while (token != NULL){
        temp[params_index] = token;
        token = strtok(NULL, " ");
        params_index++;
    }

    /*
        Command_index is used to keep track of the index of a full command --including parameters
        Foro example, if the user inputs "ls -l | grep 3230", then command_index will be 0 for the first command "ls -l" and 1 for the second command "grep 3230"
    */
    int command_index = 0;
    /*
        arg_index is used to keep track of the index of the start of a command. For example, if the user inputs "ls -l | grep 3230", then arg_index will be 0 for the first command "ls" and 3 for the second command "grep".
    */
    int arg_index = 0;

    /* Iterate over temp string to see if any string is a pipe "|" or a background operator "&" */
    for (int j = 0; j < params_index; j++){

        /* If the current string is a pipe, the next command goes in the next command_index */
        if (strcmp(temp[j], "|") == 0){

            /* If a pipe is the last character, simply continue */
            if (j == params_index - 1){
                continue;
            }

            /* If the next string is also a pipe, exit with an error */
            if (strcmp(temp[j+1], "|") == 0){
                printf("3230shell: should not have two consecutive | without in-between command\n");
                cmd->argc = -1;
                return;
            }

            if (strcmp(temp[j+1], "&") == 0){
                printf("3230shell: should not have & after |\n");
                cmd->argc = -1;
                return;
            }

            command_index++;
            arg_index = 0;
            continue;
        }

        cmd->argv[command_index][arg_index] = temp[j];
        arg_index++;
    }

    free(input);

    cmd->argc = command_index + 1;
}


int execute(struct command *cmd){


    /* if the command is exit, then exit the shell */
    if (strcmp(cmd->argv[0][0], "exit") == 0){
        /* If there are any other parameters, return error code */
        if (cmd->argv[0][1] != NULL){
            printf("3230shell: \"exit\" with other arguments!!!\n");
            return -1;
        }
        exit(0);

    /* If the command is cd, change working directory*/
    } else if (strcmp(cmd->argv[0][0], "cd") == 0){
        if (strlen(cmd->argv[0][0]) == 1){
            chdir(getenv("HOME"));
        } else {
            chdir(cmd->argv[0][1]);
        }
        return 0;
    }


    /* For every cmd->argv line, create a pipe and connect pipe. For the last pipe, connect it to the current program. */
    int pipes[cmd->argc][2];
    enum {RD, WR};
    pid_t pids[cmd->argc];

    /* Finally, prepare some buffer for the parent program */
    int nbytes;
    char readbuffer[10000] = {};

    /* Create the pipe the processes are going to output to */
    for (int z=0; z < cmd->argc; z++){
        if (pipe(pipes[z]) < 0){
            perror("pipe error");
        }
    }

    int i;
    for (i = 0; i < cmd->argc; i++){

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
            /* Set STDERR to also output to the pipe */
            if(dup2(pipes[i][WR], STDERR_FILENO) < 0){
                printf("Unable to set STDERR\n");
                exit(1);
            }

            /* Close the pipes */
            for (int z=0; z < cmd->argc; z++){
                close(pipes[z][RD]);
                close(pipes[z][WR]);
            }

            /* Set the signal catchers */
            char *type = "child";
            install_catcher(SIGKILL, type);
            install_catcher(SIGINT, type);

            /* Finally, simply execute the program */
            execvp(cmd->argv[i][0], cmd->argv[i]);
            /* Exit with an error if the program is unable to be executed */
            exit(127);
        }
    }

    struct rusage rusage;
    int status;
    int ret;
    /* Once the child processes have been created, close all pipes except the last one */
    for (int z=0; z < cmd->argc-1; z++){
        if (cmd->timex[z] == 1){
            ret = wait4(pids[cmd->argc-1], &status, 0, &rusage);
            printf("user time: %.6f, system time: %.6f\n",
            rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec / 1000000.0,
            rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec / 1000000.0);
        }
        close(pipes[z][RD]);
        close(pipes[z][WR]);
    }

    /* Then also close the write end of the last pipe */
    close(pipes[cmd->argc-1][WR]);




    /* Then, we obviously need to wait for the last process to finish */
    ret = wait4(pids[cmd->argc-1], &status, 0, &rusage);
    if (cmd->timex[cmd->argc-1] == 1){
        ret = wait4(pids[cmd->argc-1], &status, 0, &rusage);
        printf("user time: %.6f, system time: %.6f\n",
        rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec / 1000000.0,
        rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec / 1000000.0);
    }

    /* And read the buffer */
    nbytes = read(pipes[cmd->argc-1][RD], readbuffer, sizeof(readbuffer));
    printf("%s", readbuffer);
    fflush(stdout);

    /* Now we can check the exit status of the executed processes and print out any errors */

    for (int i=0; i<cmd->argc; i++){

        /* If the status of the child is not an error, exit with that code*/
        waitpid(pids[i], &status, 0);

        /* Otherwise print message to screen and exit */
        if (WEXITSTATUS(status) == 255) {
            if (access(cmd->argv[i][0], F_OK) == 0) {
                // file exists
                printf("'%s': Permission denied\n", cmd->argv[i][0]);
                return 127;
            } else {
                // file doesn't exist
                printf("'%s': No such file or directory\n", cmd->argv[i][0]);
                return 127;
            }
        } else if (WEXITSTATUS(status) == 127) {

            printf("'%s': Command not found\n", cmd->argv[i][0]);
            return 127;
        }
    }
    return WEXITSTATUS(status);
}


// get command line arguments
int main(int argc, char *argv[]) {

    // Register signal handler
    char *type = "shell";
    if (install_catcher(SIGINT, type)) {
        fprintf(stderr, "Cannot install signal handler: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    while (1) {

        /* Declare/reinitialize variables so we can reuse them */
        char    *line = NULL;

        /* Print the prompt */
        printf ( "$$ 3230shell ## " );
        fflush(stdout);

        /* Initialize comand structure */
        struct command cmd={.isBackground = 0,
                            .argv = {},
                            .timex = {},
                            .argc = 0};

        /* parse command line */
        parse(line, &cmd);

        /* If the parsing fails, continue*/
        if (cmd.argc == -1){
            continue;
        }

        /* Otherwise execute parsed commands */
        execute(&cmd);

        fflush(stdin);
        /* Free & reset variables to save memory */
        free(line);
    }
}
