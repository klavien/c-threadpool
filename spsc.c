/**
 * Copyright (c) 2021, klavien 
 * All rights reserved.
 */
# include <stdlib.h>
# include <unistd.h>
# include <sys/syscall.h>
# include <string.h>
# include "spsc.h"

static __thread thread_local_info *th_loc_info=NULL;

typedef struct routine_args {
    spsc *job;
    int idx;
}routine_args;

typedef spsc_watcher task_entry;

static void read_work(spsc *job)
{
    struct reader_result res;
    int n=0;
    int read_num=0;
    void (*func)(spsc_watcher* handle);
    while((n = mqueue_reader_parpare(job->request_queue, &res)) > 0) 
    {
        for (int i=0;i<n;i++) 
        {
            task_entry *entry=mqueue_reader_next(&res);
            func=entry->function;
            entry->th_info=th_loc_info;
            func(entry);
        }
        mqueue_reader_commit(job->request_queue, &res);
        read_num+=n;
    }
    if(read_num>0)
    {
        __atomic_fetch_sub(&(job->request_count),read_num,__ATOMIC_RELAXED);
    }
}
static void *thread_routine(void *data)
{
    routine_args *args=(routine_args*)data;
    spsc *job=args->job;
    int idx=args->idx;
    free(data);
    th_loc_info=malloc(sizeof(*th_loc_info));
    if(!th_loc_info) return NULL;
    th_loc_info->idx=idx;
    while(!(job->shutdown)) {
        read_work(job);
        usleep(50);
    }
    free(th_loc_info);
    return NULL;
}
int64_t spsc_task_cnt(spsc *job)
{
    return __atomic_load_n(&(job->request_count), __ATOMIC_RELAXED);
}

int spsc_add(spsc *job,void (*function_p)(spsc_watcher*), void* arg_p)
{
    task_entry *entry=NULL;
    while(!(entry=(task_entry *)mqueue_writer_parpare(job->request_queue)));
    entry->function=function_p;
    entry->data=arg_p;
    mqueue_writer_commit(job->request_queue, entry);
    __atomic_fetch_add(&(job->request_count),1,__ATOMIC_RELAXED);
    return 0;
}

static void spsc_free(spsc *job)
{
    if(job->threads) free(job->threads);
    if(job->request_queue) mqueue_destroy(job->request_queue);
    free(job);
}

spsc *spsc_create(size_t max_task_num)
{
    spsc *job = malloc(sizeof(spsc));
    if (job == NULL) return NULL;
    memset(job, 0, sizeof(spsc));
    job->request_queue=mqueue_create((max_task_num<1024?1024:max_task_num), sizeof(task_entry));
    if(!job->request_queue)
    {
        free(job);
        return NULL;
    }
    job->thread_count = 1;
    job->threads = calloc(job->thread_count, sizeof(pthread_t));
    if (job->threads == NULL) {
        spsc_free(job);
        return NULL;
    }
    th_loc_info=malloc(sizeof(*th_loc_info));
    if(!th_loc_info)
    {
        spsc_free(job);
        return NULL;
    }
    th_loc_info->idx=-1;
    for (int i = 0; i < job->thread_count; ++i) {
        routine_args *args=malloc(sizeof(*args));
        if(!args)
        {
            spsc_release(job);
            return NULL;
        }
        args->idx=i;
        args->job=job;
        if (pthread_create(&job->threads[i], NULL, thread_routine, args) != 0) {
            spsc_release(job);
            return NULL;
        }
        job->thread_start++;
    }
    return job;
}

void spsc_release(spsc *job)
{
    if(job->shutdown) return;
    job->shutdown = 1;
    for (int i = 0; i < job->thread_start; ++i) {
        pthread_join(job->threads[i],NULL);
    }
    read_work(job);
    //printf("spsc_task_cnt:%d\n",spsc_task_cnt(job));
    if(th_loc_info) free(th_loc_info);
    spsc_free(job);
}
