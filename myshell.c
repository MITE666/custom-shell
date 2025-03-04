#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CMD 1024
#define MAX_ARGS 100

void init() {
    printf("************************************************\n");
    printf("*                                              *\n");
    printf("*          WELCOME TO MY CUSTOM SHELL          *\n");
    printf("*                                              *\n");
    printf("************************************************\n");
    fflush(stdout);
}

void help() {
    printf("\n");
    printf("    help    Shows all commands\n");
    printf("    exit    Closes the shell\n");
    printf("    You can run basic terminal commands in here as well\n");
    printf("\n");
    fflush(stdout);
}

void handle_sigint() {
    printf("mysh>");
    fflush(stdout);
}

void execute_command(char *cmd, char *args[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, args);
        perror("execvp error!\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("Forking error!\n");
        fflush(stdout);
    }
}

void process_command(char *buff) {
    buff[strcspn(buff, "\n")] = 0;

    if (!strcmp(buff, "exit")) {
        exit(0);
    }

    if (!strcmp(buff, "help")) {
        help();
        return;
    }

    char *args[MAX_ARGS];

    char *token = strtok(buff, " ");
    int i = 0;
    while (token) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (i > 0) {
        execute_command(args[0], args);
    }
}

int main(int argc, char *argv[]) {

    signal(SIGINT, handle_sigint);

    init();
    
    char buff[MAX_CMD];

    while (1) {
        printf("mysh>");
        fflush(stdout);

        if (!fgets(buff, sizeof(buff), stdin)) {
            perror("Error at getting input!\n");
            exit(1);
        }

        process_command(buff);
    }

    return 0;
}