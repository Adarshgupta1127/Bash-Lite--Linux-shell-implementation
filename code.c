/* Bash-lite: Optimized Unix Shell for Embedded Systems */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_INPUT 256
#define MAX_ARGS 64
#define HISTORY_SIZE 10

/* LRU Command History */
typedef struct {
    char commands[HISTORY_SIZE][MAX_INPUT];
    int timestamps[HISTORY_SIZE];
    int counter;
} History;

History hist = { .counter = 0 };

/* Signal Handler */
void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\n$ ", 3);
}

/* Add command to history */
void add_history(const char* cmd) {
    int lru = 0;
    for (int i = 1; i < HISTORY_SIZE; i++) {
        if (hist.timestamps[i] < hist.timestamps[lru])
            lru = i;
    }
    strncpy(hist.commands[lru], cmd, MAX_INPUT);
    hist.timestamps[lru] = hist.counter++;
}

/* Execute command with redirection */
int execute(char** args, int input_fd, int output_fd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

/* Parse and execute pipeline */
int execute_pipeline(char** cmds[], int cmd_count) {
    int fds[2];
    int prev_fd = STDIN_FILENO;
    
    for (int i = 0; i < cmd_count; i++) {
        pipe(fds);
        if (execute(cmds[i], prev_fd, i == cmd_count-1 ? STDOUT_FILENO : fds[1]) == -1)
            return -1;
        close(fds[1]);
        prev_fd = fds[0];
    }
    return 0;
}

/* Built-in commands */
int handle_builtin(char** args) {
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || chdir(args[1]) 
            perror("cd");
        return 1;
    }
    if (strcmp(args[0], "exit") == 0) 
        exit(0);
    if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (hist.commands[i][0])
                printf("%d: %s\n", hist.timestamps[i], hist.commands[i]);
        }
        return 1;
    }
    return 0;
}

/* Main shell loop */
void shell_loop() {
    char input[MAX_INPUT];
    char* args[MAX_ARGS];
    char* cmds[MAX_ARGS][MAX_ARGS];
    int cmd_count = 0;
    
    signal(SIGINT, sigint_handler);
    
    while (1) {
        printf("$ ");
        if (!fgets(input, MAX_INPUT, stdin)) break;
        
        input[strcspn(input, "\n")] = 0;
        if (!input[0]) continue;
        
        add_history(input);
        
        /* Parse pipeline */
        char* token;
        char* rest = input;
        cmd_count = 0;
        
        while ((token = strtok_r(rest, "|", &rest))) {
            int arg_count = 0;
            char* arg_token;
            char* arg_rest = token;
            
            while ((arg_token = strtok_r(arg_rest, " \t", &arg_rest))) {
                cmds[cmd_count][arg_count++] = arg_token;
            }
            cmds[cmd_count][arg_count] = NULL;
            cmd_count++;
        }
        
        if (cmd_count == 1 && handle_builtin(cmds[0])) 
            continue;
            
        execute_pipeline(cmds, cmd_count);
    }
}

int main() {
    shell_loop();
    return 0;
}