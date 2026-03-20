# Thread Pool & Job Queue — Dispatcher/Worker in C

A multithreaded dispatcher-worker system built in C using POSIX threads (`pthreads`).  
A single dispatcher thread reads job commands from a file and enqueues them into a  
shared thread-safe queue. A configurable pool of worker threads dequeues and executes  
jobs concurrently.

## How It Works

- **Dispatcher** reads a command file line by line and routes jobs to the queue
- **Worker threads** wait on a condition variable, pick up jobs, and execute them
- **No busy waiting** — workers sleep via `pthread_cond_wait` until work arrives
- **Per-counter mutexes** prevent race conditions on shared counter files

## Features

- Thread-safe job queue using `pthread_mutex` + `pthread_cond`
- Configurable number of worker threads (up to 4096)
- Per-counter file locking (up to 100 counters)
- Turnaround-time statistics: min / avg / max
- Optional per-thread logging

## Build & Run
```bash
make
./hw2 <cmdfile> <num_threads> <num_counters> <log_enabled>
```

Example:
```bash
./hw2 test.txt 4 10 1
```

## Command File Format

| Command | Description |
|---|---|
| `worker increment X` | Increment counter file X |
| `worker decrement X` | Decrement counter file X |
| `worker msleep X` | Sleep X milliseconds |
| `worker repeat X; cmd...` | Repeat the following commands X times |
| `dispatcher_wait` | Block until all queued jobs finish |
| `dispatcher_msleep X` | Sleep X milliseconds on dispatcher |
