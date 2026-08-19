#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#include "lexer.h"
#include "history.h"
#include "parser.h"
#include "expand.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge - Milestone 2.2\n");
    printf("Parser and Expand\n");
    printf("=====================================\n");

    while (1)
    {
        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        if (line[0] == '\0')
        {
            free(line);
            continue;
        }

        if (strcmp(line, "exit") == 0)
        {
            free(line);
            printf("Exiting...\n");
            break;
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

        printf("\n========== PARSED COMMAND ==========\n");

        for (int i = 0; i < cmdline.count; i++)
        {
            Command *cmd = &cmdline.commands[i];

            printf("Command %d:\n", i + 1);

            for (int j = 0; j < cmd->argc; j++)
            {
                printf("  argv[%d] = %s\n",
                       j,
                       cmd->argv[j]);
            }

            if (cmd->input_file != NULL)
            {
                printf("  input  = %s\n",
                       cmd->input_file);
            }

            if (cmd->output_file != NULL)
            {
                printf("  output = %s\n",
                       cmd->output_file);

                if (cmd->append)
                    printf("  mode   = append\n");
                else
                    printf("  mode   = overwrite\n");
            }

            if (cmd->background)
            {
                printf("  background = yes\n");
            }
        }

        printf("====================================\n");

        free_command_line(&cmdline);

        free(expanded);
        free(line);
    }

    free_history();

    return 0;
}

