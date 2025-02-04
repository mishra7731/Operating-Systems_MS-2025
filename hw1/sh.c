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
int sh_execute_pipeline(char **commands[], int num_pipes, char *input_file, char *output_file);


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

int sh_execute(char **args) {
    if (args[0] == NULL) {
        return 1;  // Empty command entered.
    }

    int num_pipes = 0;
    char **commands[SH_TOK_BUFSIZE];  // Array of command argument lists
    int cmd_index = 0;
    char *input_file = NULL, *output_file = NULL;

    // Store the first command correctly
    commands[cmd_index++] = args;

    // Scan to find `<`, `>`, or `|`
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {  // Input Redirection
            input_file = args[i + 1];
            args[i] = NULL;  // Null terminate current command
        } else if (strcmp(args[i], ">") == 0) {  // Output Redirection
            output_file = args[i + 1];
            args[i] = NULL;  // Null terminate current command
        } else if (strcmp(args[i], "|") == 0) {  // Pipe detected
            args[i] = NULL;  // Terminate current command
            commands[cmd_index++] = &args[i + 1];  // Store next command
            num_pipes++;
        }
    }

    commands[cmd_index] = NULL;  // Null terminate the commands array

    // If a pipe is found, execute the pipeline
    if (num_pipes > 0) {
        return sh_execute_pipeline((char ***)commands, num_pipes, input_file, output_file);
    }

    return sh_launch(args, input_file, output_file);  // Execute a single command
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

int sh_execute_pipeline(char ***commands, int num_pipes, char *input_file, char *output_file) {
    int pipe_fds[num_pipes][2];  // File descriptors for pipes
    pid_t pids[num_pipes + 1];   // One process per command

    // Create pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            perror("utsh: pipe failed");
            return 1;
        }
    }

    // Fork processes for each command
    for (int i = 0; i <= num_pipes; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {  // Child process

            // Handle input redirection for the first command
            if (i == 0 && input_file) {
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("utsh: error opening input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // Handle output redirection for the last command
            if (i == num_pipes && output_file) {
                int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd == -1) {
                    perror("utsh: error opening output file");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            // If not the first command, set stdin to previous pipe read-end
            if (i > 0) {
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
            }

            // If not the last command, set stdout to next pipe write-end
            if (i < num_pipes) {
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }

            // Close all pipes in child
            for (int j = 0; j < num_pipes; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }

            // Execute the command
            if (execvp(commands[i][0], commands[i]) == -1) {
                perror("utsh");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close all pipes in parent process
    for (int i = 0; i < num_pipes; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // Wait for all child processes
    for (int i = 0; i <= num_pipes; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 1;  // Continue shell loop
}