#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <termios.h>

#include "executor.h"
#include "builtin.h"
#include "jobs.h"

static pid_t shell_pgid;
static struct termios shell_tmodes;

static volatile sig_atomic_t child_event = 0;

/*
 * SIGCHLD handler.
 *
 * Only set a flag here.
 * Job-table operations are performed outside
 * the signal handler.
 */
static void sigchld_handler(int sig)
{
    (void)sig;

    child_event = 1;
}

/*
 * Initialize interactive job control.
 */
void setup_job_control(void)
{
    shell_pgid = getpid();

    /*
     * Shell ignores interactive terminal signals.
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    /*
     * Put ShellForge into its own process group.
     */
    if (setpgid(shell_pgid, shell_pgid) < 0)
    {
        if (errno != EPERM)
            perror("setpgid");
    }

    /*
     * Give terminal control to ShellForge.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0)
        perror("tcsetpgrp");

    /*
     * Save terminal settings.
     */
    if (tcgetattr(STDIN_FILENO, &shell_tmodes) < 0)
        perror("tcgetattr");
}

/*
 * Install SIGCHLD handler.
 */
void setup_background_handler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);

    /*
     * Do not use SA_NOCLDSTOP because Ctrl+Z
     * must generate SIGCHLD.
     */
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) < 0)
        perror("sigaction");
}

/*
 * Set up input/output redirection.
 */
static int setup_redirection(Command *cmd)
{
    if (cmd->input_file != NULL)
    {
        int fd = open(cmd->input_file, O_RDONLY);

        if (fd < 0)
        {
            perror(cmd->input_file);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    if (cmd->output_file != NULL)
    {
        int flags = O_WRONLY | O_CREAT;

        if (cmd->append)
            flags |= O_APPEND;
        else
            flags |= O_TRUNC;

        int fd = open(cmd->output_file, flags, 0644);

        if (fd < 0)
        {
            perror(cmd->output_file);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

/*
 * Give terminal control to a process group.
 */
static void give_terminal_to(pid_t pgid)
{
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
        perror("tcsetpgrp");
}

/*
 * Return terminal control to ShellForge.
 */
static void return_terminal_to_shell(void)
{
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0)
        perror("tcsetpgrp");

    if (tcgetattr(STDIN_FILENO, &shell_tmodes) < 0)
        perror("tcgetattr");
}

/*
 * Wait for a foreground process group.
 *
 * WUNTRACED allows Ctrl+Z to be detected.
 */
static void wait_for_foreground_job(pid_t pgid, int job_id)
{
    int status;
    pid_t pid;

    while (1)
    {
        pid = waitpid(-pgid, &status, WUNTRACED);

        if (pid < 0)
        {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD)
                break;

            perror("waitpid");
            break;
        }

        /*
         * Ctrl+Z stopped the foreground process group.
         */
        if (WIFSTOPPED(status))
        {
            if (job_id > 0)
                job_stop(job_id);

            break;
        }

        /*
         * One process in the group finished.
         *
         * Continue waiting because a pipeline may
         * contain multiple processes.
         */
        if (WIFEXITED(status) || WIFSIGNALED(status))
            continue;
    }

    /*
     * Give terminal back to ShellForge.
     */
    return_terminal_to_shell();
}

/*
 * Reap completed background jobs.
 *
 * Jobs are checked by process-group ID rather than
 * calling getpgid() on a child after it has already
 * been reaped.
 */
void reap_background_jobs(void)
{
    int i;

    if (!child_event)
        return;

    child_event = 0;

    for (i = 1; i <= MAX_JOBS; i++)
    {
        Job *job = job_find(i);

        if (job == NULL)
            continue;

        if (job->pgid <= 0)
            continue;

        while (1)
        {
            int status;
            pid_t result;

            result = waitpid(
                -job->pgid,
                &status,
                WNOHANG | WUNTRACED
            );

            if (result == 0)
            {
                /*
                 * Processes are still running.
                 */
                break;
            }

            if (result < 0)
            {
                if (errno == EINTR)
                    continue;

                if (errno == ECHILD)
                {
                    /*
                     * No processes remain in this job.
                     */
                    if (job->state != JOB_STOPPED)
                        job->state = JOB_DONE;

                    break;
                }

                break;
            }

            if (WIFSTOPPED(status))
            {
                job->state = JOB_STOPPED;
                break;
            }

            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                /*
                 * There may be more processes in a pipeline.
                 * Continue checking the process group.
                 */
                continue;
            }
        }
    }
}

/*
 * Execute a single command.
 */
static void execute_single_command(Command *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return;

    /*
     * Foreground builtins execute directly
     * inside the shell.
     */
    if (!cmd->background &&
        is_builtin(cmd->argv[0]) &&
        cmd->input_file == NULL &&
        cmd->output_file == NULL)
    {
        execute_builtin(cmd->argv);
        return;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        /*
         * CHILD PROCESS
         */

        /*
         * Create a new process group.
         */
        if (setpgid(0, 0) < 0)
            perror("setpgid");

        /*
         * Restore normal terminal signals.
         */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        /*
         * Apply redirection.
         */
        if (setup_redirection(cmd) != 0)
            _exit(EXIT_FAILURE);

        /*
         * Builtin executed in child.
         */
        if (is_builtin(cmd->argv[0]))
        {
            int result = execute_builtin(cmd->argv);

            _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }

        /*
         * Execute external command.
         */
        execvp(cmd->argv[0], cmd->argv);

        perror(cmd->argv[0]);
        _exit(EXIT_FAILURE);
    }

    /*
     * PARENT PROCESS
     */

    /*
     * Put child into its own process group.
     */
    if (setpgid(pid, pid) < 0)
    {
        if (errno != EACCES && errno != ESRCH)
            perror("setpgid");
    }

    /*
     * BACKGROUND COMMAND
     */
    if (cmd->background)
    {
        int job_id = job_add(
            pid,
            JOB_RUNNING,
            cmd->argv[0]
        );

        if (job_id > 0)
        {
            printf("[%d] %d\n", job_id, pid);
            fflush(stdout);
        }

        return;
    }

    /*
     * FOREGROUND COMMAND
     *
     * Register it so Ctrl+Z can mark it STOPPED.
     */
    int job_id = job_add(
        pid,
        JOB_RUNNING,
        cmd->argv[0]
    );

    /*
     * Give terminal to foreground process.
     */
    give_terminal_to(pid);

    /*
     * Wait for foreground process.
     */
    wait_for_foreground_job(pid, job_id);

    /*
     * Keep stopped jobs.
     * Remove completed jobs.
     */
    Job *job = job_find(job_id);

    if (job != NULL && job->state != JOB_STOPPED)
    {
        job_remove(job_id);
    }
}

/*
 * Execute a pipeline.
 *
 * Every process in the pipeline belongs to
 * the same process group.
 *
 * Therefore a pipeline is treated as ONE job.
 */
static void execute_pipeline(CommandLine *cmdline)
{
    int previous_read = -1;
    pid_t pgid = 0;

    if (cmdline == NULL || cmdline->count == 0)
        return;

    /*
     * The final command determines whether
     * the complete pipeline runs in background.
     */
    int background =
        cmdline->commands[cmdline->count - 1].background;

    /*
     * Create every process in the pipeline.
     */
    for (int i = 0; i < cmdline->count; i++)
    {
        Command *cmd = &cmdline->commands[i];

        if (cmd->argc == 0)
            continue;

        int pipefd[2] = {-1, -1};

        /*
         * Create pipe except for final command.
         */
        if (i < cmdline->count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");

                if (previous_read != -1)
                    close(previous_read);

                return;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (previous_read != -1)
                close(previous_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            return;
        }

        if (pid == 0)
        {
            /*
             * CHILD PROCESS
             */

            /*
             * First process becomes process-group leader.
             */
            if (pgid == 0)
            {
                if (setpgid(0, 0) < 0)
                    perror("setpgid");
            }
            else
            {
                /*
                 * Remaining processes join the same group.
                 */
                if (setpgid(0, pgid) < 0)
                    perror("setpgid");
            }

            /*
             * Restore normal terminal signals.
             */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            /*
             * Previous command -> stdin.
             */
            if (previous_read != -1)
            {
                if (dup2(previous_read, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }
            }

            /*
             * Current command -> next command.
             */
            if (pipefd[1] != -1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }
            }

            /*
             * Close unused descriptors.
             */
            if (previous_read != -1)
                close(previous_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            /*
             * Apply redirection.
             */
            if (setup_redirection(cmd) != 0)
                _exit(EXIT_FAILURE);

            /*
             * Builtin inside pipeline.
             */
            if (is_builtin(cmd->argv[0]))
            {
                int result = execute_builtin(cmd->argv);

                _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
            }

            /*
             * Execute external command.
             */
            execvp(cmd->argv[0], cmd->argv);

            perror(cmd->argv[0]);
            _exit(EXIT_FAILURE);
        }

        /*
         * PARENT PROCESS
         */

        /*
         * First child becomes process-group ID.
         */
        if (pgid == 0)
            pgid = pid;

        /*
         * Parent also places child in process group.
         */
        if (setpgid(pid, pgid) < 0)
        {
            if (errno != EACCES && errno != ESRCH)
                perror("setpgid");
        }

        /*
         * Parent no longer needs previous read end.
         */
        if (previous_read != -1)
            close(previous_read);

        /*
         * Parent never writes into pipe.
         */
        if (pipefd[1] != -1)
            close(pipefd[1]);

        /*
         * Save read end for next command.
         */
        previous_read = pipefd[0];

        if (i == cmdline->count - 1)
            previous_read = -1;
    }

    /*
     * Close remaining descriptor.
     */
    if (previous_read != -1)
        close(previous_read);

    /*
     * BACKGROUND PIPELINE
     *
     * Entire pipeline is one job.
     */
    if (background)
    {
        int job_id = job_add(
            pgid,
            JOB_RUNNING,
            cmdline->commands[0].argv[0]
        );

        if (job_id > 0)
        {
            printf("[%d] %d\n", job_id, pgid);
            fflush(stdout);
        }

        return;
    }

    /*
     * FOREGROUND PIPELINE
     *
     * Entire pipeline is one job.
     */
    int job_id = job_add(
        pgid,
        JOB_RUNNING,
        cmdline->commands[0].argv[0]
    );

    /*
     * Give terminal to entire pipeline.
     */
    give_terminal_to(pgid);

    /*
     * Wait for entire foreground pipeline.
     */
    wait_for_foreground_job(pgid, job_id);

    /*
     * Keep stopped jobs.
     * Remove completed jobs.
     */
    Job *job = job_find(job_id);

    if (job != NULL && job->state != JOB_STOPPED)
    {
        job_remove(job_id);
    }
}

/*
 * Main command execution function.
 */
void execute_command_line(CommandLine *cmdline)
{
    if (cmdline == NULL || cmdline->count == 0)
        return;

    /*
     * Single command.
     */
    if (cmdline->count == 1)
    {
        execute_single_command(&cmdline->commands[0]);
        return;
    }

    /*
     * Multiple commands connected by pipes.
     */
    execute_pipeline(cmdline);
}

