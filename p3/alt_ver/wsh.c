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

// typedef struct job
// {
//     pid_t pid;          // Process ID
//     char *command;      // Command
//     bool is_bg;         // true if running bg
//     bool in_use;        // true if in use
//     long counter_value; // counter's value when this job is added
// } job_t;

/* A process is a single process.  */
typedef struct process
{
    struct process *next; /* next process in pipeline */
    char **argv;          /* for exec */
    pid_t pid;            /* process ID */
    char completed;       /* true if process has completed */
    char stopped;         /* true if process has stopped */
    int status;           /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
    struct job *next;          /* next active job */
    char *command;             /* command line, used for messages */
    process *first_process;    /* list of processes in this job */
    pid_t pgid;                /* process group ID */
    char notified;             /* true if user told about stopped job */
    struct termios tmodes;     /* saved terminal modes */
    int stdin, stdout, stderr; /* standard i/o channels */
} job;

/* The active jobs are linked into a list.  This is its head.   */
job *first_job = NULL;
/* Keep track of attributes of the shell.  */

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

long counter = 1;
/* Find the active job with the indicated pgid.  */
job *find_job(pid_t pgid)
{
    job *j;

    for (j = first_job; j; j = j->next)
        if (j->pgid == pgid)
            return j;
    return NULL;
}

/* Return true if all processes in the job have stopped or completed.  */
int job_is_stopped(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        if (!p->completed && !p->stopped)
            return 0;
    return 1;
}

/* Return true if all processes in the job have completed.  */
int job_is_completed(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        if (!p->completed)
            return 0;
    return 1;
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */

int mark_process_status(pid_t pid, int status)
{
    job *j;
    process *p;

    if (pid > 0)
    {
        /* Update the record for the process.  */
        for (j = first_job; j; j = j->next)
            for (p = j->first_process; p; p = p->next)
                if (p->pid == pid)
                {
                    p->status = status;
                    if (WIFSTOPPED(status))
                        p->stopped = 1;
                    else
                    {
                        p->completed = 1;
                        if (WIFSIGNALED(status))
                            fprintf(stderr, "%d: Terminated by signal %d.\n",
                                    (int)pid, WTERMSIG(p->status));
                    }
                    return 0;
                }
        fprintf(stderr, "No child process %d.\n", pid);
        return -1;
    }
    else
    {
        /* Other weird errors.  */
        perror("waitpid");
        return -1;
    }
}

/* Check for processes that have status information available,
   without blocking.  */

void update_status(void)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
    while (!mark_process_status(pid, status));
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */

void wait_for_job(job *j)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    while (!mark_process_status(pid, status) && !job_is_stopped(j) && !job_is_completed(j));
}

/* Format information about job status for the user to look at.  */

void format_job_info(job *j, const char *status)
{
    fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void do_job_notification(void)
{
    job *j, *jlast, *jnext;

    /* Update status information for child processes.  */
    update_status();

    jlast = NULL;
    for (j = first_job; j; j = jnext)
    {
        jnext = j->next;

        /* If all processes have completed, tell the user the job has
           completed and delete it from the list of active jobs.  */
        if (job_is_completed(j))
        {
            format_job_info(j, "completed");
            if (jlast)
                jlast->next = jnext;
            else
                first_job = jnext;
            free_job(j);
        }

        /* Notify the user about stopped jobs,
           marking them so that we won’t do this more than once.  */
        else if (job_is_stopped(j) && !j->notified)
        {
            format_job_info(j, "stopped");
            j->notified = 1;
            jlast = j;
        }

        /* Don’t say anything about jobs that are still running.  */
        else
            jlast = j;
    }
}

/* Put job j in the foreground.  If cont is nonzero,
   restore the saved terminal modes and send the process group a
   SIGCONT signal to wake it up before we block.  */

void put_job_in_foreground(job *j, int cont)
{
    /* Put the job into the foreground.  */
    tcsetpgrp(shell_terminal, j->pgid);

    /* Send the job a continue signal, if necessary.  */
    if (cont)
    {
        tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    }

    /* Wait for it to report.  */
    wait_for_job(j);

    /* Put the shell back in the foreground.  */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Restore the shell’s terminal modes.  */
    tcgetattr(shell_terminal, &j->tmodes);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

/* Put a job in the background.  If the cont argument is true, send
   the process group a SIGCONT signal to wake it up.  */

void put_job_in_background(job *j, int cont)
{
    /* Send the job a continue signal, if necessary.  */
    if (cont)
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
}

/* Mark a stopped job J as being running again.  */

void mark_job_as_running(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        p->stopped = 0;
    j->notified = 0;
}

/* Continue the job J.  */

void continue_job(job *j, int foreground)
{
    mark_job_as_running(j);
    if (foreground)
        put_job_in_foreground(j, 1);
    else
        put_job_in_background(j, 1);
}

/* Make sure the shell is running interactively as the foreground job
   before proceeding. */

void init_shell()
{

    /* See if we are running interactively.  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive)
    {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Ignore interactive and job-control signals.  */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0)
        {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}
void launch_process(process *p, pid_t pgid,
                    int infile, int outfile, int errfile,
                    int foreground)
{
    pid_t pid;

    if (shell_is_interactive)
    {
        /* Put the process into the process group and give the process group
           the terminal, if appropriate.
           This has to be done both by the shell and in the individual
           child processes because of potential race conditions.  */
        pid = getpid();
        if (pgid == 0)
            pgid = pid;
        setpgid(pid, pgid);
        if (foreground)
            tcsetpgrp(shell_terminal, pgid);

        /* Set the handling for job control signals back to the default.  */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }

    /* Set the standard input/output channels of the new process.  */
    if (infile != STDIN_FILENO)
    {
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if (outfile != STDOUT_FILENO)
    {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    if (errfile != STDERR_FILENO)
    {
        dup2(errfile, STDERR_FILENO);
        close(errfile);
    }

    /* Exec the new process.  Make sure we exit.  */
    execvp(p->argv[0], p->argv);
    perror("execvp");
    exit(1);
}

void launch_job(job *j, int foreground)
{
    process *p;
    pid_t pid;
    int mypipe[2], infile, outfile;

    infile = j->stdin;
    for (p = j->first_process; p; p = p->next)
    {
        /* Set up pipes, if necessary.  */
        if (p->next)
        {
            if (pipe(mypipe) < 0)
            {
                perror("pipe");
                exit(1);
            }
            outfile = mypipe[1];
        }
        else
            outfile = j->stdout;

        /* Fork the child processes.  */
        pid = fork();
        if (pid == 0)
            /* This is the child process.  */
            launch_process(p, j->pgid, infile,
                           outfile, j->stderr, foreground);
        else if (pid < 0)
        {
            /* The fork failed.  */
            perror("fork");
            exit(1);
        }
        else
        {
            /* This is the parent process.  */
            p->pid = pid;
            if (shell_is_interactive)
            {
                if (!j->pgid)
                    j->pgid = pid;
                setpgid(pid, j->pgid);
            }
        }

        /* Clean up after pipes.  */
        if (infile != j->stdin)
            close(infile);
        if (outfile != j->stdout)
            close(outfile);
        infile = mypipe[0];
    }

    format_job_info(j, "launched");

    if (!shell_is_interactive)
        wait_for_job(j);
    else if (foreground)
        put_job_in_foreground(j, 0);
    else
        put_job_in_background(j, 0);
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