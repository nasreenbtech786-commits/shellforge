#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#include "lexer.h"
#include "history.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge - Milestone 3.1\n");
    printf("Built-in Commands\n");
    printf("=====================================\n");

    while (1)
    {
        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\n");
            break;
        }

        if (line[0] == '\0')
        {
            free(line);
            continue;
        }

        if (strcmp(line, "history") == 0)
        {
            show_history();
            free(line);
            continue;
        }

        add_history_entry(line);

        char *expanded = expand_variables(line);

        if (expanded == NULL)
        {
            free(line);
            continue;
        }

        CommandLine cmdline = {0};

        parser(expanded, &cmdline);

        if (cmdline.count > 0 &&
            cmdline.commands[0].argc > 0 &&
            is_builtin(cmdline.commands[0].argv[0]))
        {
            int should_exit =
                execute_builtin(cmdline.commands[0].argv);

            free_command_line(&cmdline);
            free(expanded);
            free(line);

            if (should_exit)
                break;

            continue;
        }

        free_command_line(&cmdline);
        free(expanded);
        free(line);
    }

    free_history();

    return 0;
}


