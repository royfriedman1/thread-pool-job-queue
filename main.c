#include "hw2.h"

// Global variables definition
int g_num_threads;
int g_num_counters;
int g_log_enabled;

// Declaration of global start time defined in functions.c
extern long long start_time_ms;

int main(int argc, char *argv[])
{
    // 1. Argument validation and parsing
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <cmdfile> <num_threads> <num_counters> <log_enabled>\n", argv[0]);
        return 1;
    }

    char *cmdfile_name = argv[1]; // Command file name
    g_num_threads = atoi(argv[2]);
    g_num_counters = atoi(argv[3]);
    g_log_enabled = atoi(argv[4]);

    // 2. Initialization

    // a. Create counter files
    create_counter_files(g_num_counters);

    // b. Initialize work queue, locks, and time variables
    init_queue();

    // c. Open Dispatcher log file (if enabled)
    FILE *disp_log = NULL;
    if (g_log_enabled)
    {
        disp_log = fopen("dispatcher.txt", "w");
    }

    // d. Create Worker threads
    pthread_t *threads = malloc(sizeof(pthread_t) * g_num_threads);
    if (!threads)
    {
        perror("Failed to allocate threads memory");
        return 1;
    }

    for (int i = 0; i < g_num_threads; i++)
    {
        int *id = malloc(sizeof(int));
        if (!id)
        {
            perror("Failed to allocate thread ID");
            return 1;
        }
        *id = i;
        if (pthread_create(&threads[i], NULL, worker_routine, id) != 0)
        {
            perror("Failed to create thread");
            return 1;
        }
    }

    // 3. Process command file (Dispatcher Loop)
    FILE *cmd_file = fopen(cmdfile_name, "r");
    if (!cmd_file)
    {
        perror("Error opening command file");
        return 1;
    }

    char line[MAX_CMD_LEN];
    while (fgets(line, sizeof(line), cmd_file))
    {

        // Remove newline character and trim
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0)
            continue;

        // Dispatcher log (line read)
        if (g_log_enabled && disp_log)
        {
            long long now = get_current_time_ms();
            fprintf(disp_log, "TIME %lld: read cmd line: %s\n", now - start_time_ms, line);
            fflush(disp_log);
        }

        // --- Command type check ---
        if (strncmp(line, "dispatcher_msleep", 17) == 0)
        {
            int ms;
            sscanf(line, "dispatcher_msleep %d", &ms);
            usleep(ms * 1000); // Sleep in milliseconds
        }
        else if (strncmp(line, "dispatcher_wait", 15) == 0)
        {
            // Synchronous wait for all pending jobs to complete
            pthread_mutex_lock(&g_queue.lock);
            while (g_queue.size > 0 || g_queue.active_workers > 0)
            {
                // Wait for 'all_idle' signal
                pthread_cond_wait(&g_queue.all_idle, &g_queue.lock);
            }
            pthread_mutex_unlock(&g_queue.lock);
        }
        else if (strncmp(line, "worker", 6) == 0)
        {
            // Submit job to queue (skip "worker " prefix)
            submit_job(line + 7);
        }
    }

    fclose(cmd_file);

    // 4. Shutdown and Cleanup Procedure

    // a. Final wait for any remaining tasks
    pthread_mutex_lock(&g_queue.lock);
    while (g_queue.size > 0 || g_queue.active_workers > 0)
    {
        pthread_cond_wait(&g_queue.all_idle, &g_queue.lock);
    }

    // b. Signal Workers to exit their loops
    g_queue.finish_work = 1;
    pthread_cond_broadcast(&g_queue.not_empty); // Wake everyone to check the finish flag
    pthread_mutex_unlock(&g_queue.lock);

    // c. Join threads
    for (int i = 0; i < g_num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // 5. Write statistics file (stats.txt)

    long long total_runtime = get_current_time_ms() - start_time_ms;

    FILE *stats_file = fopen("stats.txt", "w");
    if (stats_file)
    {
        fprintf(stats_file, "total running time: %lld milliseconds\n", total_runtime);

        // Data collected by Worker Threads protected by stats_lock
        fprintf(stats_file, "sum of jobs turnaround time: %lld milliseconds\n", stats_sum_turnaround);

        if (stats_total_jobs > 0)
        {
            fprintf(stats_file, "min job turnaround time: %lld milliseconds\n", stats_min_turnaround);
            fprintf(stats_file, "average job turnaround time: %f milliseconds\n", (double)stats_sum_turnaround / stats_total_jobs);
            fprintf(stats_file, "max job turnaround time: %lld milliseconds\n", stats_max_turnaround);
        }
        else
        {
            // Handle edge case: no jobs were submitted
            fprintf(stats_file, "min job turnaround time: 0 milliseconds\n");
            fprintf(stats_file, "average job turnaround time: 0.0 milliseconds\n");
            fprintf(stats_file, "max job turnaround time: 0 milliseconds\n");
        }
        fclose(stats_file);
    }

    // 6. Resource deallocation
    if (disp_log)
        fclose(disp_log);
    free(threads);

    // Destroy synchronization objects
    pthread_mutex_destroy(&g_queue.lock);
    pthread_cond_destroy(&g_queue.not_empty);
    pthread_cond_destroy(&g_queue.all_idle);
    pthread_mutex_destroy(&stats_lock);
    for (int i = 0; i < MAX_COUNTERS; i++)
    {
        pthread_mutex_destroy(&counter_locks[i]);
    }

    return 0;
}