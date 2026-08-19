#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"

int is_builtin(const char *command)
{
    if (command == NULL)
        return 0;

    if (strcmp(command, "cd") == 0 ||
        strcmp(command, "pwd") == 0 ||
        strcmp(command, "echo") == 0 ||
        strcmp(command, "exit") == 0)
    {
        return 1;
    }

    return 0;
}

int execute_builtin(char **argv)
{
    if (argv == NULL || argv[0] == NULL)
        return 0;

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
