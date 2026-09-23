#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "jobs.h"

static Job job_table[MAX_JOBS];
static int next_job_id = 1;

void jobs_init(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        job_table[i].job_id = 0;
        job_table[i].pgid = 0;
        job_table[i].state = JOB_DONE;
        job_table[i].command = NULL;
    }

    next_job_id = 1;
}

int job_add(pid_t pgid, JobState state, const char *command)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id == 0)
        {
            job_table[i].job_id = next_job_id++;
            job_table[i].pgid = pgid;
            job_table[i].state = state;

            if (command != NULL)
            {
                job_table[i].command = strdup(command);
            }
            else
            {
                job_table[i].command = strdup("");
            }

            if (job_table[i].command == NULL)
            {
                job_table[i].job_id = 0;
                job_table[i].pgid = 0;
                job_table[i].state = JOB_DONE;
                return -1;
            }

            return job_table[i].job_id;
        }
    }

    fprintf(stderr, "jobs: job table is full\n");
    return -1;
}

Job *job_find(int job_id)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id == job_id)
        {
            return &job_table[i];
        }
    }

    return NULL;
}

Job *job_find_by_pgid(pid_t pgid)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id != 0 &&
            job_table[i].pgid == pgid)
        {
            return &job_table[i];
        }
    }

    return NULL;
}

void job_remove(int job_id)
{
    Job *job;

    job = job_find(job_id);

    if (job == NULL)
    {
        return;
    }

    free(job->command);

    job->job_id = 0;
    job->pgid = 0;
    job->state = JOB_DONE;
    job->command = NULL;
}

const char *job_state_string(JobState state)
{
    if (state == JOB_RUNNING)
    {
        return "Running";
    }

    if (state == JOB_STOPPED)
    {
        return "Stopped";
    }

    if (state == JOB_DONE)
    {
        return "Done";
    }

    return "Unknown";
}

void jobs_print(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id != 0)
        {
            printf("[%d] %-8s %s\n",
                   job_table[i].job_id,
                   job_state_string(job_table[i].state),
                   job_table[i].command);
        }
    }
}

void job_stop(int job_id)
{
    Job *job;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "jobs: no such job: %d\n", job_id);
        return;
    }

    job->state = JOB_STOPPED;
}

void job_continue(int job_id)
{
    Job *job;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "jobs: no such job: %d\n", job_id);
        return;
    }

    if (kill(-job->pgid, SIGCONT) == -1)
    {
        perror("jobs: SIGCONT");
        return;
    }

    job->state = JOB_RUNNING;
}

void job_done(int job_id)
{
    Job *job;

    job = job_find(job_id);

    if (job == NULL)
    {
        return;
    }

    job->state = JOB_DONE;
}

