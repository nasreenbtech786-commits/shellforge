#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

typedef enum
{
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobState;

typedef struct
{
    int job_id;
    pid_t pgid;
    JobState state;
    char *command;
} Job;

void jobs_init(void);

int job_add(pid_t pgid, JobState state, const char *command);

Job *job_find(int job_id);

Job *job_find_by_pgid(pid_t pgid);

void job_remove(int job_id);

void jobs_print(void);

void job_stop(int job_id);

void job_continue(int job_id);

void job_done(int job_id);

const char *job_state_string(JobState state);

#endif

