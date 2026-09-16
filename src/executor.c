#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "executor.h"
#include "builtin.h"

/*
 * Set up input/output redirection for a command.
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
 * Execute a pipeline of commands.
 *
 * Example:
 *
 *     ls | grep src | wc -l
 *
 * Each command gets its own child process.
 * Pipes connect the stdout of one command
 * to the stdin of the next command.
 */
static void execute_pipeline(CommandLine *cmdline)
{
    int previous_read = -1;
    pid_t pids[MAX_COMMANDS];

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
                return;
            }
        }

        /*
         * Create a child process for this command.
         */
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

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
             * If this is not the first command,
             * connect previous pipe's read end to stdin.
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
             * If this is not the last command,
             * connect current pipe's write end to stdout.
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
             * Close file descriptors that are no longer needed.
             */
            if (previous_read != -1)
                close(previous_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            /*
             * Apply normal input/output redirection.
             */
            if (setup_redirection(cmd) != 0)
                _exit(EXIT_FAILURE);

            /*
             * Builtin commands inside a pipeline
             * execute in the child process.
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

            /*
             * execvp() returns only when an error occurs.
             */
            perror(cmd->argv[0]);
            _exit(EXIT_FAILURE);
        }

        /*
         * PARENT PROCESS
         */

        pids[i] = pid;

        /*
         * The parent no longer needs the previous
         * pipe's read end.
         */
        if (previous_read != -1)
            close(previous_read);

        /*
         * The parent does not write into the pipe.
         */
        if (pipefd[1] != -1)
            close(pipefd[1]);

        /*
         * Keep the current pipe's read end.
         * It becomes stdin for the next command.
         */
        previous_read = pipefd[0];

        /*
         * For the last command there is no next pipe.
         */
        if (i == cmdline->count - 1)
            previous_read = -1;
    }

    /*
     * Close any remaining pipe descriptor.
     */
    if (previous_read != -1)
        close(previous_read);

    /*
     * Wait for all child processes.
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
     * A single builtin without redirection
     * must execute in the parent process.
     *
     * This is important for commands such as:
     *
     *     cd /tmp
     *
     * because changing directory inside a child
     * would not change the shell's directory.
     */
    if (cmdline->count == 1)
    {
        Command *cmd = &cmdline->commands[0];

        if (cmd->argc > 0 &&
            is_builtin(cmd->argv[0]) &&
            cmd->input_file == NULL &&
            cmd->output_file == NULL)
        {
            execute_builtin(cmd->argv);
            return;
        }
    }

    /*
     * Otherwise execute the command line as a pipeline.
     */
    execute_pipeline(cmdline);

}
