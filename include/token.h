#ifndef TOKEN_H
#define TOKEN_H

typedef enum
{
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
    TOKEN_BACKGROUND,
    TOKEN_END
} TokenType;

typedef struct
{
    TokenType type;
    char *value;
} Token;

#endif

