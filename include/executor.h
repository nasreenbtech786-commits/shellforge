#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

void execute_command_line(CommandLine *cmdline);

void setup_background_handler(void);

void setup_job_control(void);

void reap_background_jobs(void);

#endif

