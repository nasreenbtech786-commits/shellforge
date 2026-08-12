#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include "history.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge\n");
    printf("A Unix Style Shell written in C\n");
    printf("=====================================\n");

    while (1)
    {
        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        if (strlen(line) == 0)
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

        printf("YOU ENTERED : %s\n", line);

        free(line);
    }

    free_history();

    return 0;
}
