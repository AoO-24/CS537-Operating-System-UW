#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

#define PROMPT "wsh> "
#define MAX_LENGTH 256
#define MAX_JOB_COUNT 256

typedef struct job
{
    pid_t pid;          // Process ID
    char *command;      // Command
    bool is_bg;         // true if running bg
    bool in_use;        // true if in use
    long counter_value; // counter's value when this job is added
} job_t;

job_t jobs[MAX_JOB_COUNT];
long counter = 1;
pid_t shellpid;
int find_next_job_id(void)
{
    // Loop through all jobs to find the first unused job slot
    for (int i = 1; i < MAX_JOB_COUNT; i++)
    {
        if (!jobs[i].in_use)
        {
            // Mark the job slot as in use and return its ID
            jobs[i].in_use = true;
            return i;
        }
    }
    // Return -1 if no unused job slot is found
    return -1;
}

int find_most_recently_added_job(void)
{
    long biggest_value = 0;
    int most_recent_job_id = -1;

    // Loop through all jobs to find the one with the highest counter value
    for (int i = 1; i < MAX_JOB_COUNT; i++)
    {
        if (jobs[i].in_use)
        {
            if (jobs[i].counter_value > biggest_value)
            {
                biggest_value = jobs[i].counter_value;
                most_recent_job_id = i;
            }
        }
    }

    return most_recent_job_id;
}
int allocate_command(int job_id, const char *command)
{
    jobs[job_id].command = strdup(command);
    if (jobs[job_id].command == NULL)
    {
        return -1;
    }
    return 0;
}

void add_job(pid_t pid, char *command, bool is_bg)
{
    int new_job_id = find_next_job_id();

    // Assign PID and background flag
    jobs[new_job_id].pid = pid;
    jobs[new_job_id].is_bg = is_bg;

    // Allocate and assign command string
    if (allocate_command(new_job_id, command) == -1)
    {
        exit(-1);
    }

    // Update counter value for the new job
    jobs[new_job_id].counter_value = counter++;
}

void remove_job(int id)
{
    // Validate job ID
    bool isInvalidID = id < 0;
    bool isJobNotInUse = !jobs[id].in_use;

    if (isInvalidID || isJobNotInUse)
    {
        return;
    }

    // Free dynamically allocated command and mark job as not in use
    free(jobs[id].command);
    jobs[id].in_use = false;
}

void handle_exit_or_signal(int pid)
{
    for (int i = 1; i < MAX_JOB_COUNT; i++)
    {
        if (jobs[i].in_use && jobs[i].pid == pid)
        {
            remove_job(i);
            tcsetpgrp(STDIN_FILENO, shellpid);
            break;
        }
    }
}

void handle_stopped(int pid)
{
    for (int i = 1; i < MAX_JOB_COUNT; i++)
    {
        if (jobs[i].in_use && jobs[i].pid == pid)
        {
            // Mark job as 'stopped' (background)
            tcsetpgrp(STDIN_FILENO, shellpid);
            break;
        }
    }
}

void sigchld_handler(int sig)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            handle_exit_or_signal(pid);
        }
        else if (WIFSTOPPED(status))
        {
            handle_stopped(pid);
        }
    }
}

void wait_for_job(int id)
{
    int status;
    waitpid(jobs[id].pid, &status, WUNTRACED);
    if (WIFSTOPPED(status))
    {
    }
    else
    {
        remove_job(id);
    }
}

void move_to_foreground(int id)
{
    if (id < 0 || id >= MAX_JOB_COUNT || !jobs[id].in_use)
    {
        return;
    }

    tcsetpgrp(STDIN_FILENO, jobs[id].pid);
    kill(jobs[id].pid, SIGCONT);

    wait_for_job(id);

    tcsetpgrp(STDIN_FILENO, shellpid);
}

bool is_valid_job(int id)
{
    return id >= 0 && id < MAX_JOB_COUNT && jobs[id].in_use;
}

void resume_job(int id)
{
    kill(jobs[id].pid, SIGCONT);
}

void move_to_background(int id)
{
    if (is_valid_job(id))
    {
        resume_job(id);
    }
}
void handle_exit(int argc)
{
    exit(argc == 1 ? 0 : -1);
}

void handle_cd(int argc, char **args)
{
    if (argc != 2)
        perror("cd");
    else if (chdir(args[1]) != 0)
        perror("cd");
}

void handle_fg(int argc, char **args)
{
    int id = (argc == 1) ? find_most_recently_added_job() : atoi(args[1]);
    if (argc > 2)
        exit(-1);
    move_to_foreground(id);
}

void handle_bg(int argc, char **args)
{
    int id = (argc == 1) ? MAX_JOB_COUNT - 1 : atoi(args[1]);
    if (argc > 2)
        exit(-1);
    move_to_background(id);
}

void handle_jobs()
{
    for (int i = 0; i < MAX_JOB_COUNT; ++i)
    {
        if (jobs[i].in_use)
        {
            printf("%d: %s%s\n", i, jobs[i].command, jobs[i].is_bg ? " &" : "");
        }
    }
}
int exec_cmd(int s_argc, char **s_args, char *line)
{
    if (s_argc == 0)
        return 0;

    const char *cmd = s_args[0];

    if (strcmp(cmd, "exit") == 0)
        handle_exit(s_argc);
    else if (strcmp(cmd, "cd") == 0)
        handle_cd(s_argc, s_args);
    else if (strcmp(cmd, "fg") == 0)
        handle_fg(s_argc, s_args);
    else if (strcmp(cmd, "bg") == 0)
        handle_bg(s_argc, s_args);
    else if (strcmp(cmd, "jobs") == 0)
        handle_jobs();
    else
    {
        pid_t pid = fork();

        if (s_args[s_argc - 1][0] == '&')
        {
            // Background process
            s_args[s_argc - 1] = NULL;
            if (pid == 0)
            {
                // Child process
                execvp(s_args[0], s_args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                // Parent process
                add_job(pid, line, true);
            }
            else
            {
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            // Foreground process
            if (pid == 0)
            {
                // Configure signal to default behavior
                struct sigaction sa_default;
                sa_default.sa_handler = SIG_DFL;
                sa_default.sa_flags = 0;
                sigemptyset(&sa_default.sa_mask);

                sigaction(SIGINT, &sa_default, NULL);
                sigaction(SIGTSTP, &sa_default, NULL);
                sigaction(SIGCHLD, &sa_default, NULL);

                // Set process group and terminal control
                setpgid(0, 0);
                tcsetpgrp(STDIN_FILENO, getpid());

                // Execute command
                execvp(s_args[0], s_args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                // Parent process
                add_job(pid, line, false);
                tcsetpgrp(STDIN_FILENO, pid);

                int status;
                waitpid(pid, &status, WUNTRACED);

                if (WIFSTOPPED(status))
                {
                    for (int i = 0; i < MAX_JOB_COUNT; i++)
                    {
                        if (jobs[i].in_use && jobs[i].pid == pid)
                        {

                            break;
                        }
                    }
                }
                else
                {
                    // Remove terminated jobs
                    for (int i = 0; i < MAX_JOB_COUNT; i++)
                    {
                        if (jobs[i].in_use && jobs[i].pid == pid)
                        {
                            remove_job(i);
                            break;
                        }
                    }
                }

                tcsetpgrp(STDIN_FILENO, shellpid);
            }
            else
            {
                exit(EXIT_FAILURE);
            }
        }
    }
    return 0;
}
void setup_pipes(int I, int pipe_count, int pipes[2][2])
{
    if (pipe_count && pipe(pipes[0]) == -1)
    {
        perror("pipe");
    }
}

void setup_io_redirection(int I, int pipe_count, int pipes[2][2])
{
    if ((I == pipe_count) || I != 0)
    {
        dup2(pipes[1][0], STDIN_FILENO);
    }
    if (I == 0 || !(I == pipe_count))
    {
        dup2(pipes[0][1], STDOUT_FILENO);
    }
}

void alternate_pipes(int pipes[2][2])
{
    int tmp0 = pipes[0][0];
    int tmp1 = pipes[0][1];

    pipes[0][0] = pipes[1][0];
    pipes[0][1] = pipes[1][1];

    pipes[1][0] = tmp0;
    pipes[1][1] = tmp1;
}

void exec_pipe(int I, int pipe_count, char **command)
{
    static int pipes[2][2];
    setup_pipes(I, pipe_count, pipes);

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        setup_io_redirection(I, pipe_count, pipes);
        execvp(command[0], command);
        perror("execvp");
        exit(1);
    }
    else
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        // Close pipes
        if ((I == pipe_count) || I != 0)
        {
            close(pipes[1][0]);
        }
        if (I == 0 || !(I == pipe_count))
        {
            close(pipes[0][1]);
        }

        alternate_pipes(pipes);
    }
}

int count_pipes(int argc_in, char **argv_in)
{
    int pipe_count = 0;
    for (int i = 0; i < argc_in; i++)
    {
        if (!strcmp(argv_in[i], "|"))
        {
            pipe_count++;
        }
    }
    return pipe_count;
}

void split_into_commands(int argc_in, char **argv_in, char ***commands)
{
    int curr_pipe = 0;
    int curr_cmd_argc = 0;
    for (int i = 0; i < argc_in; i++)
    {
        if (!strcmp(argv_in[i], "|"))
        {
            commands[curr_pipe][curr_cmd_argc] = NULL;
            curr_pipe++;
            curr_cmd_argc = 0;
        }
        else
        {
            commands[curr_pipe][curr_cmd_argc] = argv_in[i];
            curr_cmd_argc++;
        }
    }
}

int process_pipe(int argc_in, char **argv_in, char *line)
{
    if (argc_in == 0)
    {
        return 0;
    }
    else if (argc_in == 1)
    {
        return exec_cmd(argc_in, argv_in, line);
    }

    int pipe_count = count_pipes(argc_in, argv_in);

    if (pipe_count == 0)
    {
        return exec_cmd(argc_in, argv_in, line);
    }

    char ***commands = (char ***)malloc((pipe_count + 1) * sizeof(char **));
    for (int i = 0; i < pipe_count + 1; i++)
    {
        commands[i] = (char **)calloc(argc_in + 1, sizeof(char *));
    }

    split_into_commands(argc_in, argv_in, commands);

    for (int i = 0; commands[i] != NULL; i++)
    {
        exec_pipe(i, pipe_count, commands[i]);
    }

    for (int i = 0; i < pipe_count + 1; i++)
    {
        free(commands[i]);
    }
    free(commands);

    return 0;
}

int count_args(char *line)
{
    int num_args = 0;
    char *line_copy = strdup(line);
    char *token = strtok(line_copy, " ");

    if (token)
    {
        num_args = 1;
    }

    while (strtok(NULL, " "))
    {
        num_args++;
    }

    free(line_copy);
    return num_args;
}

void split_into_args(char *line, char ***args, int *num_args)
{
    char *line_copy = strdup(line);
    char *token = strtok(line_copy, " ");

    while (token)
    {
        char *token_copy = strdup(token);
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " ");
    }

    (*args)[*num_args] = NULL;
    free(line_copy);
}

int cut(char *line, char ***args, int *num_args)
{
    *num_args = count_args(line);

    *args = calloc(*num_args + 1, sizeof(char **));
    if (*args == NULL)
    {
        return -1; // Memory allocation failed
    }

    *num_args = 0;
    split_into_args(line, args, num_args);

    return 0;
}

int read_line_from_input(bool print_prompt, FILE *input, char **line, size_t *len)
{
    if (print_prompt)
    {
        printf(PROMPT);
        fflush(stdout);
    }
    return getline(line, len, input);
}

void remove_newline_char(char *line)
{
    if (line[strlen(line) - 1] == '\n')
    {
        line[strlen(line) - 1] = '\0';
    }
}
/*
In Intercative mode: print_prompt = true input = stdin
In Batch mode: print_prompt = flase input = file addr
*/

int start_wsh(bool print_prompt, FILE *input)
{
    size_t len = 0;
    char *line = NULL;

    while (read_line_from_input(print_prompt, input, &line, &len) != -1)
    {
        if (line[0] == '\n' && line[1] == '\0')
        {
            continue;
        }

        remove_newline_char(line);

        char **s_args = NULL;
        int s_argc = 0;

        if (cut(line, &s_args, &s_argc) < 0)
        {
            return -1; // report error
        }

        process_pipe(s_argc, s_args, line);
    }

    return 0;
}
int main(int argc, char *argv[])
{
    struct sigaction sa_ignore;
    // sets the signal handler for sa_ignore to SIG_IGN, which tells the system to ignore the following signals.
    sa_ignore.sa_handler = SIG_IGN;
    // sets a flag that causes interrupted system calls to be restarted.
    sa_ignore.sa_flags = SA_RESTART;
    // initializes an empty signal set for sa_ignore.sa_mask.
    sigemptyset(&sa_ignore.sa_mask);
    // set the action for signals SIGINT, SIGTSTP, SIGTTOU, and SIGTTIN to be ignored.
    sigaction(SIGINT, &sa_ignore, NULL);
    sigaction(SIGTSTP, &sa_ignore, NULL);
    sigaction(SIGTTOU, &sa_ignore, NULL);
    sigaction(SIGTTIN, &sa_ignore, NULL);

    struct sigaction sa_chld;
    // sets the signal handler for sa_chld to a function named sigchld_handler.
    sa_chld.sa_handler = sigchld_handler;
    // sets flags to restart interrupted system calls and to not receive notification when child processes stop.
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    // initializes an empty signal set for sa_chld.sa_mask.
    sigemptyset(&sa_chld.sa_mask);
    // set the action for SIGCHLD signals to be handled by sigchld_handler.
    sigaction(SIGCHLD, &sa_chld, NULL);

    memset(jobs, 0, sizeof(jobs));

    shellpid = getpid();

    if (argc == 1) // interactive mode
    {
        start_wsh(true, stdin);
    }
    if (argc == 2) // file read mode
    {
        FILE *input = fopen(argv[1], "r");
        if (input == NULL)
        {
            perror("fopen");
            exit(-1);
        }
        start_wsh(false, input);
        fclose(input);
    }
    return 0;
}