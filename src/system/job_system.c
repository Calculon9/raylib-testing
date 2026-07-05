#include "system/job_system.h"
#include <stdlib.h>
#include <string.h>

#define JOB_CAPACITY_DEFAULT 256

typedef struct Job
{
    JobFunction function;
    void *context;
    int start;
    int end;
} Job;

static Job *g_jobs = NULL;
static int g_job_count = 0;
static int g_job_capacity = 0;
static bool g_initialized = false;

bool InitJobSystem(int max_jobs)
{
    if (g_initialized)
        return true;

    if (max_jobs <= 0)
        max_jobs = JOB_CAPACITY_DEFAULT;

    g_jobs = malloc(sizeof(Job) * max_jobs);
    if (!g_jobs)
        return false;

    g_job_capacity = max_jobs;
    g_job_count = 0;
    g_initialized = true;
    return true;
}

void ShutdownJobSystem(void)
{
    if (!g_initialized)
        return;

    free(g_jobs);
    g_jobs = NULL;
    g_job_count = 0;
    g_job_capacity = 0;
    g_initialized = false;
}

bool SubmitJob(JobFunction function, void *context, int item_count, int chunk_size)
{
    if (!g_initialized || !function || item_count <= 0 || chunk_size <= 0)
        return false;

    int start = 0;
    while (start < item_count && g_job_count < g_job_capacity)
    {
        int end = start + chunk_size;
        if (end > item_count)
            end = item_count;

        g_jobs[g_job_count].function = function;
        g_jobs[g_job_count].context = context;
        g_jobs[g_job_count].start = start;
        g_jobs[g_job_count].end = end;
        g_job_count++;
        start = end;
    }

    return start >= item_count;
}

void ExecuteJobs(void)
{
    if (!g_initialized)
        return;

    for (int i = 0; i < g_job_count; ++i)
    {
        Job *job = &g_jobs[i];
        if (job->function)
            job->function(job->context, job->start, job->end);
    }
}

void ClearJobs(void)
{
    if (!g_initialized)
        return;

    g_job_count = 0;
}

size_t GetPendingJobCount(void)
{
    return (size_t)g_job_count;
}

bool IsJobSystemInitialized(void)
{
    return g_initialized;
}
