#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

static Token *tokens = NULL;
static int token_count = 0;

void free_tokens(void);

static void add_token(TokenType type, const char *value)
{
    Token *temp;

    temp = realloc(tokens, (token_count + 1) * sizeof(Token));

    if (temp == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    tokens = temp;
    tokens[token_count].type = type;

    if (value != NULL)
    {
        tokens[token_count].value = malloc(strlen(value) + 1);

        if (tokens[token_count].value == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        strcpy(tokens[token_count].value, value);
    }
    else
    {
        tokens[token_count].value = NULL;
    }

    token_count++;
}

void tokenize(const char *input)
{
    char word[1024];
    int i = 0;
    int j;

    free_tokens();

    while (input[i] != '\0')
    {
        if (input[i] == ' ' || input[i] == '\t')
        {
            i++;
            continue;
        }

        if (input[i] == '|')
        {
            add_token(TOKEN_PIPE, "|");
            i++;
        }
        else if (input[i] == '<')
        {
            add_token(TOKEN_INPUT, "<");
            i++;
        }
        else if (input[i] == '>')
        {
            if (input[i + 1] == '>')
            {
                add_token(TOKEN_APPEND, ">>");
                i += 2;
            }
            else
            {
                add_token(TOKEN_OUTPUT, ">");
                i++;
            }
        }
        else if (input[i] == '&')
        {
            add_token(TOKEN_BACKGROUND, "&");
            i++;
        }
        else
        {
            j = 0;

            while (input[i] != '\0' &&
                   input[i] != ' ' &&
                   input[i] != '\t' &&
                   input[i] != '|' &&
                   input[i] != '<' &&
                   input[i] != '>' &&
                   input[i] != '&')
            {
                word[j++] = input[i++];
            }

            word[j] = '\0';

            add_token(TOKEN_WORD, word);
        }
    }

    add_token(TOKEN_END, NULL);
}

void print_tokens(void)
{
    for (int i = 0; i < token_count; i++)
    {
        switch (tokens[i].type)
        {
            case TOKEN_WORD:
                printf("%d: WORD %s\n", i, tokens[i].value);
                break;

            case TOKEN_PIPE:
                printf("%d: PIPE |\n", i);
                break;

            case TOKEN_INPUT:
                printf("%d: INPUT <\n", i);
                break;

            case TOKEN_OUTPUT:
                printf("%d: OUTPUT >\n", i);
                break;

            case TOKEN_APPEND:
                printf("%d: APPEND >>\n", i);
                break;

            case TOKEN_BACKGROUND:
                printf("%d: BACKGROUND &\n", i);
                break;

            case TOKEN_END:
                printf("%d: END END\n", i);
                break;
        }
    }
}

void free_tokens(void)
{
    for (int i = 0; i < token_count; i++)
    {
        free(tokens[i].value);
    }

    free(tokens);

    tokens = NULL;
    token_count = 0;
}
