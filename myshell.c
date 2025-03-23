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
    
    while (*ptr) {
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
        strcpy(token + len - 1, token + len);
        tokens[*count] = NULL;
    }
}

void expand_var(char *loc, char **var, int *go_back) {
    char *expanded_var = getenv(*var);
    expanded_var = expanded_var ? expanded_var : "";
    int untouched_chars = loc - full_input;

    if (strlen(*var) + 1 < strlen(expanded_var)) {
        int size_dif = strlen(expanded_var) - strlen(*var) - 1;
        full_input = realloc(full_input, total_len + size_dif + 1);
        if (!full_input) {
            free(*var);
            free(input);
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        memmove(loc + size_dif, loc, total_len - untouched_chars + 1);
        total_len += size_dif;
        *go_back = size_dif;
    } else if (strlen(*var) + 1 > strlen(expanded_var)) {
        int size_dif = strlen(*var) + 1 - strlen(expanded_var);
        memmove(loc, loc + size_dif, total_len - untouched_chars - size_dif + 1);
        full_input = realloc(full_input, total_len - size_dif + 1);
        if (!full_input) {
            free(*var);
            free(input);
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        total_len -= size_dif;
        *go_back = -size_dif;
    }
    memcpy(loc, expanded_var, strlen(expanded_var));
}

void expand_tokens(char *tokens[], int *count) {
    for (int i = 0; i < *count; ++i) {
        char *ptr = tokens[i];
        while (ptr && *ptr != '\0') {
            if (*ptr == '$' && *(ptr + 1) == '\0') {
                ptr++;
            }
            if (*ptr == '$') {
                char *begin = ptr;
                ptr++;
                int var_size = 0;
                while (ptr && *ptr != '\0' && *ptr != '$') {
                    var_size++;
                    ptr++;
                }
                char *var = malloc(var_size + 1);
                if (!var) {
                    free(input);
                    free(full_input);
                    perror("allocating memory");
                    exit(EXIT_FAILURE);
                }
                strncpy(var, begin + 1, var_size);
                var[var_size] = '\0';
                int go_back;
                expand_var(begin, &var, &go_back);
                ptr += go_back;
                free(var);
            } else {
                ptr++;
            }
        }
    }

    for (int i = 0; i < *count; ++i) {
        if (!strcmp(tokens[i], "")) {
            for (int j = i + 1; j <= *count; ++j) {
                tokens[j - 1] = tokens[j];
            }
            (*count)--;
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

void process_command(char *args[], int *count) {
    if (*count == 1 && !strcmp(args[0], "exit")) {
        exit(EXIT_SUCCESS);
    }

    if (*count == 1 && !strcmp(args[0], "help")) {
        help();
        return;
    }

    char *equal_sign;
    if (*count == 1 && (equal_sign = strchr(args[0], '='))) {
        *equal_sign = '\0';
        char *left = args[0];
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

    if (*count == 2 && !strcmp(args[0], "unset")) {
        unsetenv(args[1]);
        return;
    }

    execute_command(args[0], args);
}

// void process_command(char **args, int length) {
//     

    // if ()

    // char *equal_sign = strchr(buff, '=');
    // if (equal_sign) {
    //     *equal_sign = '\0';
    //     setenv(buff, equal_sign + 1, 1);
    //     return;
    // }

    // if (!strncmp(buff, "unset", 5)) {
    //     unsetenv(buff + 6);
    //     return;
    // }

    // char *args[MAX_ARGS];

    // char *token = strtok(buff, " ");
    // int i = 0;
    // while (token) {
    //     if (token[0] == '$') {
    //         char *env_var = getenv(token + 1);
    //         args[i++] = env_var ? env_var : "";
    //     } else {
    //         args[i++] = token;
    //     }
    //     token = strtok(NULL, " ");
    // }
    // args[i] = NULL;

    // if (i > 0) {
    //     execute_command(args[0], args);
    // }
//}

// void execute_script(char *buff) {
//     FILE *fptr;

//     char *token = strtok(buff, " \n");
//     fptr = fopen(token, "r");

//     if (!fptr) {
//         perror(token);
//         return;
//     }

//     char *args[MAX_ARGS];
//     int i = 0;
//     while (token) { 
//         char number[MAX_CMD];
//         int err = sprintf(number, "%d", i++);
//         if (err < 0) {
//             perror("Failed to convert number to string!");
//             fclose(fptr);
//             return;
//         }

//         setenv(number, token, 1);

//         token = strtok(NULL, " \n");
//     }

//     char line[MAX_CMD];
//     while (fgets(line, MAX_CMD, fptr)) {
//         process_command(line);
//     }

//     while (i) {
//         char number[MAX_CMD];
//         int err = sprintf(number, "%d", --i);
//         if (err < 0) {
//             perror("Failed to convert number to string!");
//             return;
//         }

//         unsetenv(number);
//     }
// }


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

        tokenize(full_input, tokens, &count);

        if (count == 1 && strchr(full_input, '=') && strstr(full_input, "$$")) {
            printf("You cannot use $$\n");
        } else {
            expand_tokens(tokens, &count);

            process_command(tokens, &count);
        }

        free(input);
        free(full_input);

        
        // char *args[MAX_ARGS];
        // char *token = strtok(buff, " \n");
        // int i = 0;

        // while (token && i < MAX_ARGS - 1) {
        //     args[i++] = token;

        //     token = strtok(NULL, " \n");
        // }

        // args[i] = NULL;

        //process_command(args, i);

        // if (!strncmp(buff, "./", 2) && strstr(buff, ".mshext")) {
        //     execute_script(buff + 2);
        // } else {
        //     process_command(buff);
        // }
    }

    return 0;
}


//  TODO:
//  $0 (trebuie sa inceapa nu cu cifra)
//  de implementat recursiv
//  .mshext poate fi gasit in interior
//  expandarea in argumentele scriptului (script $abc)
//  la recursive environment conflict

//  metode:
//  parsam toate tokenurile, dupa care pasez acest array ca argument pentru process command (tot aici fac trimming and shit)
//  expandez variabilele locale
//  pentru recursie - crearea unui array de environments
