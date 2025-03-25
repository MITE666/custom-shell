#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#define ARG_MAX 100

char *input;
char *full_input;
size_t size;
size_t total_len;
int quote_count;


void execute_script(char *args[], int count);
void process_command(char *args[], int count);


void init() {
    printf("************************************************\n");
    printf("*                                              *\n");
    printf("*          WELCOME TO MY CUSTOM SHELL          *\n");
    printf("*                                              *\n");
    printf("************************************************\n");
}

void help() {
    printf("\n");
    printf("    help    Shows all commands\n");
    printf("    exit    Closes the shell\n");
    printf("    You can run basic terminal commands in here as well\n");
    printf("\n");
}

// void handle_sigint() {
//     printf("\nmysh> ");
//     fflush(stdout);
//     free(input);
//     free(full_input);
// }

int count_quotes(const char *buff) {
    int count = 0;
    const char *ptr = buff;

    while ((ptr = strchr(ptr, '"'))) {
        count++;
        ptr++;
    }

    return count;
}

void tokenize(char *input, char *tokens[], int *count) {
    char quotes[] = "\"\"";
    char *copy = input;
    while (copy = strstr(input, quotes)) {
        memmove(copy, copy + 2, strlen(copy + 2) + 1);
        total_len -= 2;
    }

    *count = 0;
    char *ptr = input;
    int quotes_begin_inside = 0;
    int quotes_end_inside = 0;
    
    while (*ptr && *count < ARG_MAX - 1) {
        int increase = 0;
        while (*ptr == ' ') {
            ptr++;
        }

        if (*ptr == '\0') break;

        if (*ptr == '"' || quotes_begin_inside) {
            ptr++;

            if (!quotes_begin_inside) tokens[*count] = ptr; 

            while (*ptr && *ptr != '"') {
                ptr++;  
            }

            if (*ptr == '"') {
                if (*(ptr + 1) && *(ptr + 1) == ' ') {
                    *ptr++ = '\0';
                    increase = 1;
                } else {
                    memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                    total_len -= 1;
                    quotes_end_inside = 1;
                }
                quotes_begin_inside = 0;
            }  
        } else {
            if (!quotes_end_inside) {
                tokens[*count] = ptr;  
            }
            while (*ptr && *ptr != ' ' && *ptr != '"') {
                ptr++; 
            }
            
            if (*ptr != '"') {
                if (*ptr) *ptr++ = '\0';  
                increase = 1;
            } else {
                memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                total_len -= 1;
                quotes_begin_inside = 1;
            }
            
            quotes_end_inside = 0;
        }

        (*count) += increase;
    }

    if (!strcmp(tokens[*count - 1], "\n")) {
        tokens[--(*count)] = NULL;
    } else {
        char *token = tokens[*count - 1];
        int len = strlen(token);
        if (token[len - 1] == '\n') {
            strcpy(token + len - 1, token + len);
        }
        tokens[*count] = NULL;
    }
}

void expand_var(char **buff, char *loc, char *exp_var, int size_dif) {
    int untouched_chars = loc - *buff;

    if (size_dif > 0) {
        *buff = realloc(*buff, strlen(*buff) + size_dif + 1);
        if (!(*buff)) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        memmove(loc + size_dif, loc, strlen(*buff) - untouched_chars + 1);
    } else if (size_dif < 0) {
        memmove(loc, loc - size_dif, strlen(*buff) - untouched_chars + size_dif + 1);
        *buff = realloc(*buff, strlen(*buff) + size_dif + 1);
        if (!(*buff)) {
            free(input);
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    memcpy(loc, exp_var, strlen(exp_var));
}

void expand_tokens(char **input) {
    char *ptr = *input;
    while (*ptr != '\0') {
        if (*ptr == '$' && (*(ptr + 1) == '\0' || *(ptr + 1) == '\n')) {
            ptr++;
        }
        if (*ptr == '$') {
            char *begin = ptr;
            ptr++;
            int var_size = 0;
            while (!isspace((unsigned char)*ptr) && *ptr != '$' && *ptr != '\0') {
                var_size++;
                ptr++;
            }
            char var[var_size + 1];
            strncpy(var, begin + 1, var_size);
            var[var_size] = '\0';
            char *expanded_var = getenv(var);
            expanded_var = expanded_var ? expanded_var : "";
            int size_dif = strlen(expanded_var) - var_size - 1;

            expand_var(input, begin, expanded_var, size_dif);
            ptr += size_dif;
        } else {
            ptr++;
        }
    }
}


void expand_file_args(char **buff, char *args[], int args_count) {
    char *ptr = *buff;
    while (*ptr != '\0') {
        if (*ptr == '$' && isdigit(*(ptr + 1))) {
            char *begin = ptr;
            ptr++;
            int num_size = 0;
            while (isdigit(*ptr)) {
                ptr++;
                num_size++;
            }
            if (num_size > 3) continue;
            if (!isspace((unsigned char)*ptr) && *ptr != '\0') continue;
            char num_str[4];
            strncpy(num_str, begin + 1, num_size);
            num_str[num_size] = '\0';
            char *endptr;
            long num = strtol(num_str, &endptr, 10);

            if (*endptr != '\0') continue;
            
            char *arg = (num >= args_count - 1) ? "" : args[num + 1];
            int size_dif = strlen(arg) - num_size - 1;

            expand_var(buff, begin, arg, size_dif);
            ptr += size_dif;
        } else {
            ptr++;
        }
    }
}

void execute_command(char *cmd, char *args[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, args);
        perror(cmd);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("Forking error!\n");
    }
}

void execute_script(char *args[], int count) {
    pid_t pid = fork();
    if (pid == 0) {
        FILE *fptr;

        fptr = fopen(args[0], "r");
    
        if (!fptr) {
            perror(args[0]);
            return;
        }
    
        char *line_input = NULL;
        size_t line_size = 0;
        size_t total_line_size = 0;
        ssize_t read;
    
        char *tokens[ARG_MAX];
        int tokens_count;
    
        while ((read = getline(&line_input, &line_size, fptr)) != -1) {
            total_line_size = read;
    
            expand_file_args(&line_input, args, count);
    
            expand_tokens(&line_input);
    
            tokenize(line_input, tokens, &tokens_count);
    
            process_command(tokens, tokens_count);
    
            line_size = 0;
    
            free(line_input);
        }
    
        if (ferror(fptr)) {
            perror("reading file line");
            free(line_input);
            free(input);
            free(full_input);
            exit(EXIT_FAILURE);
        }
    
        fclose(fptr);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("Forking error");
    }
}

void process_command(char *args[], int count) {
    if (count == 1 && !strcmp(args[0], "exit")) {
        exit(EXIT_SUCCESS);
    }

    if (count == 1 && !strcmp(args[0], "help")) {
        help();
        return;
    }

    char *equal_sign;
    if (count == 1 && (equal_sign = strchr(args[0], '='))) {
        *equal_sign = '\0';
        char *left = args[0];

        if (isdigit(*left)) {
            printf("Variable cannot start with a digit.\n");
            return;
        }

        char *right = equal_sign + 1;
        while (right && *right != '\0') {
            if (*right == '\n' || *right == ' ') {
                *right = ' ';
                right++;
            }
            int entered_while = 0;
            while (right && isspace((unsigned char)*right)) {
                memmove(right, right + 1, strlen(right + 1) + 1);
                total_len -= 1;
                entered_while = 1;
            }
            if (!entered_while) {
                right++;
            }
        }
        right--;
        if (*right == ' ') {
            memmove(right, right + 1, strlen(right + 1) + 1);
            total_len -= 1;
        }
        right = equal_sign + 1;
        setenv(left, right, 1);
        return;
    }

    if (count == 2 && !strcmp(args[0], "unset")) {
        unsetenv(args[1]);
        return;
    }

    if (!strncmp(args[0], "./", 2) && !strcmp(args[0] + strlen(args[0]) - strlen(".mshext"), ".mshext")) {
        execute_script(args, count);
        return;
    }

    execute_command(args[0], args);
}


int main(int argc, char *argv[]) {

    //signal(SIGINT, handle_sigint);

    init();

    while (1) {

        input = NULL;
        full_input = NULL;
        size = 0;
        total_len = 0;
        quote_count = 0;
        
        printf("mysh> ");
        fflush(stdout);

        while(1) {
            ssize_t read = getline(&input, &size, stdin);

            if (read == -1) {
                perror("reading input");
                break;
            }

            size_t new_size = total_len + read + 1;

            full_input = realloc(full_input, new_size);

            if (!full_input) {
                perror("reallocating memory");
                free(input);
                exit(EXIT_FAILURE);
            }

            quote_count += count_quotes(input);

            strcpy(full_input + total_len, input);
            total_len += read;

            if (quote_count % 2 == 0) {
                break;
            }

            printf("> ");
            fflush(stdout);
        }

        char *tokens[ARG_MAX];
        int count;


        if (count == 1 && strchr(full_input, '=') && strstr(full_input, "$$")) {
            printf("You cannot use $$\n");
        } else {
            expand_tokens(&full_input);

            tokenize(full_input, tokens, &count);

            process_command(tokens, count);
        }

        free(input);
        free(full_input);
    }

    return 0;
}
