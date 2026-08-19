#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expand.h"

char *expand_variables(const char *input)
{
    if (input == NULL)
        return NULL;

    size_t size = strlen(input) + 1;

    char *result = malloc(size);

    if (result == NULL)
    {
        perror("malloc");
        return NULL;
    }

    result[0] = '\0';

    size_t i = 0;

    while (input[i] != '\0')
    {
        if (input[i] == '$')
        {
            i++;

            if (input[i] == '\0')
            {
                size_t len = strlen(result);

                if (len + 2 > size)
                {
                    size *= 2;
                    result = realloc(result, size);

                    if (result == NULL)
                        return NULL;
                }

                strcat(result, "$");
                break;
            }

            char variable[256];
            size_t j = 0;

            while (input[i] != '\0' &&
                   (input[i] == '_' ||
                    (input[i] >= 'a' && input[i] <= 'z') ||
                    (input[i] >= 'A' && input[i] <= 'Z') ||
                    (input[i] >= '0' && input[i] <= '9')))
            {
                if (j < sizeof(variable) - 1)
                    variable[j++] = input[i];

                i++;
            }

            variable[j] = '\0';

            if (j == 0)
            {
                size_t len = strlen(result);

                if (len + 2 > size)
                {
                    size *= 2;
                    result = realloc(result, size);

                    if (result == NULL)
                        return NULL;
                }

                strcat(result, "$");
                continue;
            }

            const char *value = getenv(variable);

            if (value == NULL)
                value = "";

            size_t needed =
                strlen(result) + strlen(value) + 1;

            while (needed > size)
                size *= 2;

            char *temp = realloc(result, size);

            if (temp == NULL)
            {
                free(result);
                return NULL;
            }

            result = temp;

            strcat(result, value);
        }
        else
        {
            size_t len = strlen(result);

            if (len + 2 > size)
            {
                size *= 2;

                char *temp = realloc(result, size);

                if (temp == NULL)
                {
                    free(result);
                    return NULL;
                }

                result = temp;
            }

            size_t current_len = strlen(result);

            result[current_len] = input[i];
            result[current_len + 1] = '\0';

            i++;
        }
    }

    return result;
}

