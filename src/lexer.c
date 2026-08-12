#include <stdio.h>
#include "lexer.h"

void lexer(const char *input)
{
    tokenize(input);
    print_tokens();
}

