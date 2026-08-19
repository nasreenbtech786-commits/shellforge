#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "executor.h"
#include "builtin.h"

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

void execute_command_line(CommandLine *cmdline)
{
    if (cmdline == NULL || cmdline->count == 0)
        return;

    for (int i = 0; i < cmdline->count; i++)
    {
        Command *cmd = &cmdline->commands[i];

        if (cmd->argc == 0)
            continue;

        /*
         * Execute a builtin directly when there is no redirection.
         * This is required for cd to change the shell's directory.
         */
        if (cmdline->count == 1 &&
            is_builtin(cmd->argv[0]) &&
            cmd->input_file == NULL &&
            cmd->output_file == NULL)
        {
            execute_builtin(cmd->argv);
            continue;
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
             * Child process
             */

            if (setup_redirection(cmd) != 0)
                exit(EXIT_FAILURE);

            /*
             * Builtins with redirection run in the child.
             * This allows echo > file to work.
             */
            if (is_builtin(cmd->argv[0]))
            {
                execute_builtin(cmd->argv);
                exit(EXIT_SUCCESS);
            }

            execvp(cmd->argv[0], cmd->argv);

            perror(cmd->argv[0]);
            exit(EXIT_FAILURE);
        }

        /*
         * Parent process
         */
        if (!cmd->background)
        {
            waitpid(pid, NULL, 0);
        }
    }
}


