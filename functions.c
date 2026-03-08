#include "hw2.h"

// Global variables definition
work_queue_t g_queue;
pthread_mutex_t counter_locks[MAX_COUNTERS];
long long start_time_ms = 0;

// Initialize global statistics variables (defined here as per declaration)
pthread_mutex_t stats_lock;
long long stats_sum_turnaround = 0;
// LLONG_MAX is the maximum possible value, ensuring any measured time will be smaller.
long long stats_min_turnaround = LLONG_MAX;
long long stats_max_turnaround = 0;
int stats_total_jobs = 0;

// --- General Helper Functions ---

// Helper function: returns current time in milliseconds
long long get_current_time_ms()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

void create_counter_files(int num_counters)
{
    for (int i = 0; i < num_counters; i++)
    {
        char filename[20];
        // Create filename in countxx.txt format
        sprintf(filename, "count%02d.txt", i);
        FILE *f = fopen(filename, "w");
        if (f)
        {
            // Initialize to 0 in long long format
            fprintf(f, "0");
            fclose(f);
        }
    }
}

// --- Queue Management ---

void init_queue()
{
    g_queue.head = NULL;
    g_queue.tail = NULL;
    g_queue.size = 0;
    g_queue.active_workers = 0;
    g_queue.finish_work = 0;

    // Initialize Condition Variables and Mutexes
    pthread_mutex_init(&g_queue.lock, NULL);
    pthread_cond_init(&g_queue.not_empty, NULL);
    pthread_cond_init(&g_queue.all_idle, NULL);

    // Initialize locks for files and statistics
    for (int i = 0; i < MAX_COUNTERS; i++)
    {
        pthread_mutex_init(&counter_locks[i], NULL);
    }
    pthread_mutex_init(&stats_lock, NULL);

    start_time_ms = get_current_time_ms();
}

// Add a job to the queue (called by Dispatcher)
void submit_job(char *line)
{
    // 1. Create a new job node
    job_t *new_job = (job_t *)malloc(sizeof(job_t));
    if (!new_job)
    {
        perror("Malloc failed");
        exit(1);
    }

    strncpy(new_job->cmd_line, line, MAX_CMD_LEN);
    new_job->next = NULL;
    // Store the entry time for statistics calculation
    new_job->entry_time = get_current_time_ms();

    // 2. Lock the queue and insert the job
    pthread_mutex_lock(&g_queue.lock);

    if (g_queue.tail == NULL)
    {
        g_queue.head = new_job;
        g_queue.tail = new_job;
    }
    else
    {
        g_queue.tail->next = new_job;
        g_queue.tail = new_job;
    }
    g_queue.size++;

    // 3. Signal a sleeping Worker that work has been added
    pthread_cond_signal(&g_queue.not_empty);

    pthread_mutex_unlock(&g_queue.lock);
}

// --- Worker Logic and File Operations ---

// Atomic (mutex-protected) counter file update
void update_counter(int counter_id, int value_change)
{
    if (counter_id < 0 || counter_id >= g_num_counters)
        return;

    char filename[64];
    sprintf(filename, "count%02d.txt", counter_id);

    // 1. Lock the specific counter mutex
    pthread_mutex_lock(&counter_locks[counter_id]);

    long long val = 0;

    // 2. READ STEP: Open for reading only
    FILE *f_read = fopen(filename, "r");
    if (f_read)
    {
        if (fscanf(f_read, "%lld", &val) != 1)
        {
            val = 0;
        }
        fclose(f_read);
    }
    // If file doesn't exist yet, val stays 0 (logic holds)

    // 3. MODIFY
    val += value_change;

    // 4. WRITE STEP: Open for writing (truncates file to 0 length first!)
    FILE *f_write = fopen(filename, "w");
    if (!f_write)
    {
        perror("Error opening counter file for write");
        pthread_mutex_unlock(&counter_locks[counter_id]);
        return;
    }

    fprintf(f_write, "%lld", val);
    fclose(f_write);

    // 5. Release the mutex
    pthread_mutex_unlock(&counter_locks[counter_id]);
}

// Parses and executes the list of commands within a single Job
void execute_job_logic(char *cmd_line, int thread_id)
{
    // Create a copy so strtok_r doesn't break the original Job string
    char *line_copy = strdup(cmd_line);
    char *saveptr1;

    // Split into base commands using semicolon (;)
    char *cmd = strtok_r(line_copy, ";", &saveptr1);

    while (cmd != NULL)
    {
        // Skip leading spaces
        while (*cmd == ' ')
            cmd++;

        char cmd_name[32];
        int arg = 0;

        // Attempt to parse: command word + number (e.g., "increment 5")
        if (sscanf(cmd, "%s %d", cmd_name, &arg) >= 1)
        {

            if (strcmp(cmd_name, "msleep") == 0)
            {
                // Milliseconds * 1000 = Microseconds
                usleep(arg * 1000);
            }
            else if (strcmp(cmd_name, "increment") == 0)
            {
                update_counter(arg, 1);
            }
            else if (strcmp(cmd_name, "decrement") == 0)
            {
                update_counter(arg, -1);
            }
            else if (strcmp(cmd_name, "repeat") == 0)
            {
                // REPEAT logic: recursively execute the rest of the line
                // saveptr1 contains everything remaining after the current command
                char *rest_of_line = saveptr1;
                if (rest_of_line)
                {
                    for (int i = 0; i < arg; i++)
                    {
                        // Recursive call to execute remaining commands X times
                        execute_job_logic(rest_of_line, thread_id);
                    }
                }
                // Stop processing after repeat, as the recursive loop handled the rest of the line
                break;
            }
        }

        // Move to the next command in the line
        cmd = strtok_r(NULL, ";", &saveptr1);
    }

    free(line_copy);
}

// --- Main Worker Thread Routine ---

void *worker_routine(void *arg)
{
    int thread_id = *(int *)arg;
    free(arg);

    // Open a log file for the thread if logging is enabled
    FILE *log_file = NULL;
    char log_name[32];
    if (g_log_enabled)
    {
        sprintf(log_name, "thread%02d.txt", thread_id);
        log_file = fopen(log_name, "w");
    }

    while (1)
    {
        // 1. Wait for work / check for termination signal
        pthread_mutex_lock(&g_queue.lock);

        // If queue is empty and no finish signal, go to sleep
        while (g_queue.size == 0 && !g_queue.finish_work)
        {
            pthread_cond_wait(&g_queue.not_empty, &g_queue.lock);
        }

        // If finish signal is received (finish_work=1) and queue is empty, exit
        if (g_queue.finish_work && g_queue.size == 0)
        {
            pthread_mutex_unlock(&g_queue.lock);
            break;
        }

        // 2. Extract job from queue
        job_t *job = g_queue.head;
        g_queue.head = job->next;
        if (g_queue.head == NULL)
            g_queue.tail = NULL;
        g_queue.size--;
        g_queue.active_workers++; // Marker: Thread is now busy
        pthread_mutex_unlock(&g_queue.lock);

        // 3. Execute work (outside of Queue Mutex)
        long long start = get_current_time_ms();

        // Log: START
        if (log_file)
        {
            // Calculate time elapsed since program start
            fprintf(log_file, "TIME %lld: START job %s\n", start - start_time_ms, job->cmd_line);
            fflush(log_file);
        }

        execute_job_logic(job->cmd_line, thread_id);

        long long end = get_current_time_ms();

        // Log: END
        if (log_file)
        {
            fprintf(log_file, "TIME %lld: END job %s\n", end - start_time_ms, job->cmd_line);
            fflush(log_file);
        }

        // 4. Update statistics and cleanup
        long long turnaround = end - job->entry_time;

        // Update global statistics (must be mutex-protected)
        pthread_mutex_lock(&stats_lock);
        stats_sum_turnaround += turnaround;
        if (turnaround < stats_min_turnaround)
            stats_min_turnaround = turnaround;
        if (turnaround > stats_max_turnaround)
            stats_max_turnaround = turnaround;
        stats_total_jobs++;
        pthread_mutex_unlock(&stats_lock);

        free(job); // Free job memory

        // 5. Signal completion and notify Dispatcher (if waiting)
        pthread_mutex_lock(&g_queue.lock);
        g_queue.active_workers--; // Marker: Thread is now idle

        // If queue is empty and no threads are active, notify the Dispatcher
        if (g_queue.size == 0 && g_queue.active_workers == 0)
        {
            pthread_cond_broadcast(&g_queue.all_idle);
        }
        pthread_mutex_unlock(&g_queue.lock);
    }

    if (log_file)
        fclose(log_file);
    return NULL;
}
