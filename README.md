# OS HW2 – Multithreaded Dispatcher-Worker

Operating Systems course (TAU) – Homework 2.

## Overview

A multithreaded dispatcher-worker system in C. A single dispatcher thread reads job commands from stdin and enqueues them into a shared thread-safe queue. A configurable pool of worker threads dequeues and executes jobs concurrently.

**Key features:**
- Thread-safe job queue using `pthread_mutex` and `pthread_cond`
- Configurable number of worker threads (up to 4096)
- Shared counters with per-counter locking
- Turnaround-time statistics (min / max / avg)
- Optional logging

## Files

| File | Description |
|------|-------------|
| `main.c` | Entry point – parses args, spawns threads, drives the dispatcher loop |
| `functions.c` | Queue operations, worker routine, counter management, job execution |
| `hw2.h` | Shared types and declarations |
| `Makefile` | Build system |
| `test.txt` | Sample input file for testing |
| `solution.pdf` | Written solution document |

## Build & Run

```bash
make
./hw2 <num_threads> <num_counters> [log]
```

Example:
```bash
./hw2 4 10 < test.txt
./hw2 4 10 log < test.txt
```
