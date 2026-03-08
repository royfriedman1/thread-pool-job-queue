#ifndef HW2_H
#define HW2_H

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <limits.h> // Required for LLONG_MAX

// Constants
#define MAX_CMD_LEN 1024
#define MAX_THREADS 4096
#define MAX_COUNTERS 100

// Job structure
typedef struct job
{
    char cmd_line[MAX_CMD_LEN];
    long long entry_time; // The time when the Dispatcher read the line
    struct job *next;
} job_t;

// Global Queue structure
typedef struct
{
    job_t *head;
    job_t *tail;
    int size;           // Current number of jobs in the queue
    int active_workers; // Number of workers currently processing a job
    int finish_work;    // Flag signaling workers to exit (1 = finish)

    pthread_mutex_t lock;     // Protects queue access
    pthread_cond_t not_empty; // Signaled when a new job is added
    pthread_cond_t all_idle;  // Signaled when queue is empty and all workers are idle
} work_queue_t;

// Global configuration variables
extern int g_num_threads;
extern int g_num_counters;
extern int g_log_enabled;
extern work_queue_t g_queue;
extern pthread_mutex_t counter_locks[MAX_COUNTERS];

// --- Statistics Variables ---
extern pthread_mutex_t stats_lock;
extern long long stats_sum_turnaround;
extern long long stats_min_turnaround;
extern long long stats_max_turnaround;
extern int stats_total_jobs;

// Function declarations
void init_queue();
void submit_job(char *line);
void *worker_routine(void *arg);
void create_counter_files(int num_counters);
long long get_current_time_ms();

// Helper functions
void update_counter(int counter_id, int value_change);
void execute_job_logic(char *cmd_line, int thread_id);

#endif // HW2_H