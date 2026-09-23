#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#include "lexer.h"
#include "history.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"
#include "jobs.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge - Milestone 5.1\n");
    printf("Job Control: jobs, fg and bg\n");
    printf("=====================================\n");

    /*
     * Initialize the job table.
     */
    jobs_init();

    /*
     * Initialize shell process group,
     * terminal control and signal handling.
     */
    setup_job_control();

    /*
     * Install SIGCHLD handler.
     */
    setup_background_handler();

    while (1)
    {
        /*
         * Reap completed background jobs
         * before displaying the next prompt.
         */
        reap_background_jobs();

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

        /*
         * History command.
         */
        if (strcmp(line, "history") == 0)
        {
            show_history();
            free(line);
            continue;
        }

        /*
         * Store command in history.
         */
        add_history_entry(line);

        /*
         * Expand environment variables.
         */
        char *expanded = expand_variables(line);

        if (expanded == NULL)
        {
            free(line);
            continue;
        }

        /*
         * Parse command.
         */
        CommandLine cmdline = {0};

        parser(expanded, &cmdline);

        /*
         * Execute command.
         */
        execute_command_line(&cmdline);

        /*
         * Free parser memory.
         */
        free_command_line(&cmdline);

        free(expanded);
        free(line);
    }

    /*
     * Clean up history.
     */
    free_history();

    return 0;
}

