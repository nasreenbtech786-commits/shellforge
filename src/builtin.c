#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "builtin.h"
#include "jobs.h"

int is_builtin(const char *command)
{
    if (command == NULL)
        return 0;

    if (strcmp(command, "cd") == 0 ||
        strcmp(command, "pwd") == 0 ||
        strcmp(command, "echo") == 0 ||
        strcmp(command, "exit") == 0 ||
        strcmp(command, "jobs") == 0 ||
        strcmp(command, "fg") == 0 ||
        strcmp(command, "bg") == 0)
    {
        return 1;
    }

    return 0;
}

int execute_job_builtin(char **argv)
{
    if (argv == NULL || argv[0] == NULL)
        return 0;

    /* jobs */
    if (strcmp(argv[0], "jobs") == 0)
    {
        if (argv[1] != NULL)
        {
            fprintf(stderr, "jobs: too many arguments\n");
            return 0;
        }

        jobs_print();
        return 0;
    }

    /* fg */
    if (strcmp(argv[0], "fg") == 0)
    {
        int job_id;
        Job *job;
        int status;
        pid_t result;

        if (argv[1] == NULL)
        {
            fprintf(stderr, "fg: job id required\n");
            return 0;
        }

        if (argv[2] != NULL)
        {
            fprintf(stderr, "fg: too many arguments\n");
            return 0;
        }

        job_id = atoi(argv[1]);

        if (job_id <= 0)
        {
            fprintf(stderr, "fg: invalid job id\n");
            return 0;
        }

        job = job_find(job_id);

        if (job == NULL)
        {
            fprintf(stderr, "fg: no such job: %d\n", job_id);
            return 0;
        }

        if (kill(-job->pgid, SIGCONT) == -1)
        {
            perror("fg: SIGCONT");
            return 0;
        }

        job->state = JOB_RUNNING;

        if (tcsetpgrp(STDIN_FILENO, job->pgid) == -1)
        {
            perror("fg: tcsetpgrp");
        }

        do
        {
            result = waitpid(-job->pgid, &status, WUNTRACED);
        }
        while (result > 0 &&
               !WIFEXITED(status) &&
               !WIFSIGNALED(status) &&
               !WIFSTOPPED(status));

        if (WIFSTOPPED(status))
        {
            job->state = JOB_STOPPED;
        }
        else
        {
            job_remove(job_id);
        }

        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        {
            perror("fg: tcsetpgrp");
        }

        return 0;
    }

    /* bg */
    if (strcmp(argv[0], "bg") == 0)
    {
        int job_id;

        if (argv[1] == NULL)
        {
            fprintf(stderr, "bg: job id required\n");
            return 0;
        }

        if (argv[2] != NULL)
        {
            fprintf(stderr, "bg: too many arguments\n");
            return 0;
        }

        job_id = atoi(argv[1]);

        if (job_id <= 0)
        {
            fprintf(stderr, "bg: invalid job id\n");
            return 0;
        }

        job_continue(job_id);

        return 0;
    }

    return 0;
}

int execute_builtin(char **argv)
{
    if (argv == NULL || argv[0] == NULL)
        return 0;

    /* Job-control builtins */
    if (strcmp(argv[0], "jobs") == 0 ||
        strcmp(argv[0], "fg") == 0 ||
        strcmp(argv[0], "bg") == 0)
    {
        return execute_job_builtin(argv);
    }

    /* cd */
    if (strcmp(argv[0], "cd") == 0)
    {
        const char *dir;

        if (argv[1] == NULL)
        {
            dir = getenv("HOME");

            if (dir == NULL)
            {
                fprintf(stderr, "cd: HOME not set\n");
                return 0;
            }
        }
        else
        {
            dir = argv[1];
        }

        if (argv[2] != NULL)
        {
            fprintf(stderr, "cd: too many arguments\n");
            return 0;
        }

        if (chdir(dir) != 0)
            perror("cd");

        return 0;
    }

    /* pwd */
    if (strcmp(argv[0], "pwd") == 0)
    {
        char cwd[4096];

        if (argv[1] != NULL)
        {
            fprintf(stderr, "pwd: too many arguments\n");
            return 0;
        }

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("pwd");
        }
        else
        {
            printf("%s\n", cwd);
        }

        return 0;
    }

    /* echo */
    if (strcmp(argv[0], "echo") == 0)
    {
        int i = 1;

        while (argv[i] != NULL)
        {
            printf("%s", argv[i]);

            if (argv[i + 1] != NULL)
                printf(" ");

            i++;
        }

        printf("\n");

        return 0;
    }

    /* exit */
    if (strcmp(argv[0], "exit") == 0)
    {
        if (argv[1] != NULL)
        {
            fprintf(stderr, "exit: too many arguments\n");
            return 0;
        }

        return 1;
    }

    return 0;
}

