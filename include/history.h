#ifndef HISTORY_H
#define HISTORY_H

#define MAX_HISTORY 100

void add_history_entry(const char *command);
void show_history(void);
void free_history(void);

#endif
