#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

void sh_loop(void); 
char *sh_read_line(void); 
char **sh_split_line(char *line); 
int sh_launch(char **args, char *input_file, char *output_file);
int sh_execute(char **args);
int sh_execute_pipe(char **cmd1, char **cmd2);

int main(int argc, char **argv)
{
  sh_loop();
  return EXIT_SUCCESS;
}

void sh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("utsh$ ");
    line = sh_read_line();
    args = sh_split_line(line);
    status = sh_execute(args);

    free(line);
    free(args);
  } while (status);
}

char *sh_read_line(void)
{
  char *line = NULL;
  size_t bufsize = 0;  // have getline allocate a buffer for us

  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin))  // EOF
    {
      fprintf(stderr, "EOF\n");
      exit(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "Value of errno: %d\n", errno);
      exit(EXIT_FAILURE);
    }
  }
  return line;
}


#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

char **sh_split_line(char *line)
{
  int bufsize = SH_TOK_BUFSIZE;
  int position = 0;
  char **tokens = malloc(bufsize * sizeof(char *));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "sh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, SH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += SH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char *));
      if (!tokens) {
        free(tokens_backup);
        fprintf(stderr, "sh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, SH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

int sh_execute(char **args) 
{
    if (args[0] == NULL) {
      return 1;  // An empty command was entered.
    }
    int i = 0, pipe_index = -1;
    char *input_file = NULL, *output_file = NULL;

    // Scan to find '<' or '>' or '|'
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {  // Input Redirection
            input_file = args[i + 1];
            args[i] = NULL;  // Remove '<' from arguments
        } 
        else if (strcmp(args[i], ">") == 0) {  // Output Redirection
            output_file = args[i + 1];
            args[i] = NULL;  // Remove '>' from arguments
        }
        else if (strcmp(args[i], "|") == 0) {  // Pipe detected
            pipe_index = i;
            args[i] = NULL;  // Split command at "|"

        }
        i++;
    }
    // If a pipe is found, process it separately
    if (pipe_index != -1) {
        return sh_execute_pipe(args, &args[pipe_index + 1]);
    }

    return sh_launch(args, input_file, output_file);   // launch
}
    

int sh_launch(char **args, char *input_file, char *output_file) {
    pid_t pid, wpid;
    int status;
    int in_fd, out_fd; 

    pid = fork(); 
    if (pid == 0) {  // Child process

        // Handle input redirection
        if (input_file) {
            in_fd = open(input_file, O_RDONLY);
            if (in_fd == -1) {
                perror("utsh: error opening input file");
                exit(EXIT_FAILURE);
            }
            dup2(in_fd, STDIN_FILENO);  // Redirect stdin
            close(in_fd);
        }

        // Handle output redirection
        if (output_file) {
            out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("utsh: error opening output file");
                exit(EXIT_FAILURE);
            }
            dup2(out_fd, STDOUT_FILENO);  // Redirect stdout
            close(out_fd);
        }

        // execution 
        if (execvp(args[0], args) == -1) {  
            perror("utsh");  // If exec fails
        }
        exit(EXIT_FAILURE);  
        
    } else if (pid < 0) {
        // Fork error
        perror("utsh: error forking");
    } else {
        // waiting for the child to finish
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1; // Continue shell loop
}

int sh_execute_pipe(char **cmd1, char **cmd2) {
    int pipefd[2];
    pid_t pid1, pid2;

    if (pipe(pipefd) == -1) {  // Create a pipe
        perror("utsh: pipe failed");
        return 1;
    }

    pid1 = fork();
    if (pid1 == 0) {  // First child (writes to pipe)
        close(pipefd[0]);  // Close read end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
        close(pipefd[1]);

        if (execvp(cmd1[0], cmd1) == -1) {
            perror("utsh");
            exit(EXIT_FAILURE);
        }
    }

    pid2 = fork();
    if (pid2 == 0) {  // Second child (reads from pipe)
        close(pipefd[1]);  // Close write end
        dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe
        close(pipefd[0]);

        if (execvp(cmd2[0], cmd2) == -1) {
            perror("utsh");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: close pipe ends and wait for children
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 1;  // Continue shell loop
}
