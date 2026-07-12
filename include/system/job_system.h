#ifndef JOB_SYSTEM_H
#define JOB_SYSTEM_H

#include <stddef.h>
#include <stdbool.h>

typedef void (*JobFunction)(void *context, int start, int end);

bool InitJobSystem(int max_jobs);
void ShutdownJobSystem(void);
bool SubmitJob(JobFunction function, void *context, int item_count, int chunk_size);
void ExecuteJobs(void);
void ClearJobs(void);
bool IsJobSystemInitialized(void);

#endif // JOB_SYSTEM_H