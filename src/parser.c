#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "token.h"

static char *duplicate_string(const char *str)
{
    if (str == NULL)
        return NULL;

    char *copy = malloc(strlen(str) + 1);

    if (copy == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(copy, str);

    return copy;
}

void free_command_line(CommandLine *cmdline)
{
    if (cmdline == NULL)
        return;

    for (int i = 0; i < cmdline->count; i++)
    {
        Command *cmd = &cmdline->commands[i];

        for (int j = 0; j < cmd->argc; j++)
        {
            free(cmd->argv[j]);
            cmd->argv[j] = NULL;
        }

        free(cmd->input_file);
        free(cmd->output_file);

        cmd->input_file = NULL;
        cmd->output_file = NULL;

        cmd->argc = 0;
        cmd->append = 0;
        cmd->background = 0;
    }

    cmdline->count = 0;
}

void parser(const char *input, CommandLine *cmdline)
{
    int token_count;
    const Token *tokens;

    if (input == NULL || cmdline == NULL)
        return;

    cmdline->count = 0;

    tokenize(input);

    tokens = get_tokens(&token_count);

    if (tokens == NULL || token_count == 0)
        return;

    Command *current = &cmdline->commands[0];

    current->argc = 0;
    current->input_file = NULL;
    current->output_file = NULL;
    current->append = 0;
    current->background = 0;

    cmdline->count = 1;

    for (int i = 0; i < token_count; i++)
    {
        const Token *token = &tokens[i];

        if (token->type == TOKEN_END)
            break;

        switch (token->type)
        {
            case TOKEN_WORD:

                if (current->argc < MAX_ARGS - 1)
                {
                    current->argv[current->argc] =
                        duplicate_string(token->value);

                    current->argc++;

                    current->argv[current->argc] = NULL;
                }

                break;

            case TOKEN_INPUT:

                if (i + 1 < token_count &&
                    tokens[i + 1].type == TOKEN_WORD)
                {
                    i++;

                    current->input_file =
                        duplicate_string(tokens[i].value);
                }

                break;

            case TOKEN_OUTPUT:

                if (i + 1 < token_count &&
                    tokens[i + 1].type == TOKEN_WORD)
                {
                    i++;

                    current->output_file =
                        duplicate_string(tokens[i].value);

                    current->append = 0;
                }

                break;

            case TOKEN_APPEND:

                if (i + 1 < token_count &&
                    tokens[i + 1].type == TOKEN_WORD)
                {
                    i++;

                    current->output_file =
                        duplicate_string(tokens[i].value);

                    current->append = 1;
                }

                break;

            case TOKEN_BACKGROUND:

                current->background = 1;

                break;

            case TOKEN_PIPE:

                if (cmdline->count < MAX_COMMANDS)
                {
                    current =
                        &cmdline->commands[cmdline->count];

                    current->argc = 0;
                    current->input_file = NULL;
                    current->output_file = NULL;
                    current->append = 0;
                    current->background = 0;

                    cmdline->count++;
                }

                break;

            default:
                break;
        }
    }
}
