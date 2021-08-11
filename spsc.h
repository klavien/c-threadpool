#ifndef __SPSC_THREAD_POOL_H__
#define __SPSC_THREAD_POOL_H__
/**
 * Copyright (c) 2021, klavien 
 * All rights reserved.
 */
# include <stdint.h>
# include <stdbool.h>
# include <pthread.h>
# include "mqueue.h"

/* spsc is a thread pool object, all threads are workers.
 * it include an job queue, you can add job to the queue,
 * workers will get job from queue and do the job. */
typedef struct spsc {
    int shutdown;
    int thread_count;
    int thread_start;
    pthread_t *threads;
    struct mqueue *request_queue;
    volatile int64_t request_count; // not exactly right,but enough to use
} spsc;

typedef struct thread_local_info{
    int idx;
}thread_local_info;

typedef struct spsc_watcher spsc_watcher;

struct spsc_watcher{
    union 
    {
        void (*function)(spsc_watcher* handle);
        thread_local_info *th_info;
    };
    void *data; // user data
};

spsc *spsc_create(size_t max_task_num);
int spsc_add(spsc *job,void (*function_p)(spsc_watcher*), void* arg_p);
int64_t spsc_task_cnt(spsc *job);
void spsc_release(spsc *job);

#endif