#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "executor.h"
#include "builtin.h"

/*
 * SIGCHLD handler.
 *
 * Background processes must be reaped when they finish.
 * WNOHANG prevents the shell from being blocked.
 */
static void sigchld_handler(int sig)
{
    int saved_errno = errno;

    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        /*
         * Reap all finished child processes.
         */
    }

    errno = saved_errno;
}

/*
 * Install the SIGCHLD handler.
 */
void setup_background_handler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);

    /*
     * Restart interrupted system calls.
     * Do not generate SIGCHLD for stopped children.
     */
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) < 0)
    {
        perror("sigaction");
    }
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
 * Execute a single command.
 *
 * Foreground command:
 *     fork -> execute -> wait
 *
 * Background command:
 *     fork -> execute -> do not wait
 */
static void execute_single_command(Command *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return;

    /*
     * A foreground builtin must execute in the shell process.
     *
     * This is required for commands such as:
     *
     *     cd /tmp
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

        if (setup_redirection(cmd) != 0)
            _exit(EXIT_FAILURE);

        /*
         * Builtins executed in a child are allowed for
         * background execution.
         */
        if (is_builtin(cmd->argv[0]))
        {
            int result = execute_builtin(cmd->argv);

            _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }

        /*
         * External command.
         */
        execvp(cmd->argv[0], cmd->argv);

        perror(cmd->argv[0]);
        _exit(EXIT_FAILURE);
    }

    /*
     * PARENT PROCESS
     */

    if (cmd->background)
    {
        /*
         * Do not wait for a background process.
         */
        printf("[Background PID: %d]\n", pid);
        fflush(stdout);
        return;
    }

    /*
     * Foreground command.
     */
    waitpid(pid, NULL, 0);
}

/*
 * Execute a pipeline.
 *
 * Examples:
 *
 *     ls | grep src
 *     ls | grep src | wc -l
 *
 * Background:
 *
 *     ls | grep src &
 */
static void execute_pipeline(CommandLine *cmdline)
{
    int previous_read = -1;
    pid_t pids[MAX_COMMANDS] = {0};

    if (cmdline == NULL || cmdline->count == 0)
        return;

    /*
     * A pipeline is considered background if the last
     * command contains '&'.
     */
    int background =
        cmdline->commands[cmdline->count - 1].background;

    for (int i = 0; i < cmdline->count; i++)
    {
        Command *cmd = &cmdline->commands[i];

        if (cmd->argc == 0)
            continue;

        int pipefd[2] = {-1, -1};

        /*
         * Create a pipe unless this is the last command.
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
             * Connect previous command's output
             * to this command's input.
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
             * Connect this command's output
             * to the next command.
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
             * Builtins inside a pipeline run in the child.
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

        pids[i] = pid;

        /*
         * Parent no longer needs previous read end.
         */
        if (previous_read != -1)
            close(previous_read);

        /*
         * Parent does not write to the pipe.
         */
        if (pipefd[1] != -1)
            close(pipefd[1]);

        /*
         * Keep read end for next command.
         */
        previous_read = pipefd[0];

        if (i == cmdline->count - 1)
            previous_read = -1;
    }

    /*
     * Close any remaining descriptor.
     */
    if (previous_read != -1)
        close(previous_read);

    /*
     * Background pipeline:
     *
     * Do NOT wait.
     * SIGCHLD handler will reap children.
     */
    if (background)
    {
        /*
         * Print the PID of the last process in the pipeline.
         */
        pid_t last_pid = pids[cmdline->count - 1];

        printf("[Background Pipeline PID: %d]\n", last_pid);
        fflush(stdout);

        return;
    }

    /*
     * Foreground pipeline:
     * wait for every process.
     */
    for (int i = 0; i < cmdline->count; i++)
    {
        if (pids[i] > 0)
            waitpid(pids[i], NULL, 0);
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
     *
     * This also handles:
     *
     *     sleep 5 &
     *     echo hello &
     *     cd /tmp
     */
    if (cmdline->count == 1)
    {
        execute_single_command(&cmdline->commands[0]);
        return;
    }

    /*
     * Multiple commands connected with pipes.
     */
    execute_pipeline(cmdline);
}

