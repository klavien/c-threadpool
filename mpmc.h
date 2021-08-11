# ifndef _thread_pool_h_
# define _thread_pool_h_
/**
 * Copyright (c) 2021, klavien 
 * All rights reserved.
 */
# include <stdint.h>
# include <stdbool.h>
# include <pthread.h>
# include "mqueue.h"

/* thread_pool is a thread pool object, all threads are workers.
 * it include an job queue, you can add job to the queue,
 * workers will get job from queue and do the job. */
typedef struct thread_pool {
    int shutdown;
    int thread_count;
    int thread_start;
    pthread_t *threads;
    struct mqueuebatch *request_queue;
    volatile int64_t request_count; // not exactly right,but enough to use
} thread_pool;

typedef struct thread_local_info{
    int idx;
}thread_local_info;

typedef struct thread_pool_watcher thread_pool_watcher;

struct thread_pool_watcher{
    union 
    {
        void (*function)(thread_pool_watcher* handle);
        thread_local_info *th_info;
    };
    void *data; // user data
};

thread_pool *thread_pool_create(size_t thread_count,size_t max_task_num);
int thread_pool_add(thread_pool *job,void (*function_p)(thread_pool_watcher*), void* arg_p);
int64_t thread_pool_task_cnt(thread_pool *job);
void thread_pool_release(thread_pool *job);

# endif
