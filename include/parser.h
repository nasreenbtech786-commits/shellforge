#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 64
#define MAX_COMMANDS 16

typedef struct
{
    char *argv[MAX_ARGS];
    int argc;

    char *input_file;
    char *output_file;

    int append;
    int background;
} Command;

typedef struct
{
    Command commands[MAX_COMMANDS];
    int count;
} CommandLine;

void parser(const char *input, CommandLine *cmdline);

void free_command_line(CommandLine *cmdline);

#endif

