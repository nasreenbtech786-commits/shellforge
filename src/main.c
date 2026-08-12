#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include "lexer.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge - Milestone 2\n");
    printf("Tokenizer and Lexer\n");
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

        lexer(line);

        free(line);
    }

    return 0;
}
