#ifndef BUILTIN_H
#define BUILTIN_H

int is_builtin(const char *command);
int execute_builtin(char **argv);

int execute_job_builtin(char **argv);

#endif

