/**
 * Copyright (c) 2021, klavien 
 * All rights reserved.
 */
# include <stdlib.h>
# include <unistd.h>
# include <sys/syscall.h>
# include <string.h>
# include "mpmc.h"

static __thread thread_local_info *th_loc_info=NULL;

typedef struct routine_args {
    thread_pool *job;
    int idx;
}routine_args;

typedef thread_pool_watcher task_entry;

static void read_work(thread_pool *job)
{
    struct mqueuebatch_reader r;
    struct reader_result res;
    mqueuebatch_reader_init(&r, job->request_queue);
    int n=0;
    int read_num=0;
    void (*func)(thread_pool_watcher* handle);
    while((n = mqueuebatch_reader_parpare(&r, &res)) > 0) 
    {
        for (int i=0;i<n;i++) 
        {
            task_entry *entry=mqueuebatch_reader_next(&res);
            func=entry->function;
            entry->th_info=th_loc_info;
            func(entry);
        }
        mqueuebatch_reader_commit(&r, &res);
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
    thread_pool *job=args->job;
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
int64_t thread_pool_task_cnt(thread_pool *job)
{
    return __atomic_load_n(&(job->request_count), __ATOMIC_RELAXED);
}

int thread_pool_add(thread_pool *job,void (*function_p)(thread_pool_watcher*), void* arg_p)
{
    struct mqueuebatch_writer w;
    mqueuebatch_writer_init(&w,job->request_queue);
    task_entry *entry=NULL;
    while(!(entry=(task_entry *)mqueuebatch_writer_parpare(&w)));
    entry->function=function_p;
    entry->data=arg_p;
    mqueuebatch_writer_commit(&w, entry);
    __atomic_fetch_add(&(job->request_count),1,__ATOMIC_RELAXED);
    return 0;
}

static void thread_pool_free(thread_pool *job)
{
    if(job->threads) free(job->threads);
    if(job->request_queue) mqueuebatch_destroy(job->request_queue);
    free(job);
}

thread_pool *thread_pool_create(size_t thread_count,size_t max_task_num)
{
    thread_pool *job = malloc(sizeof(thread_pool));
    if (job == NULL) return NULL;
    memset(job, 0, sizeof(thread_pool));
    job->request_queue=mqueuebatch_create((max_task_num<1024?1024:max_task_num), sizeof(task_entry));
    if(!job->request_queue)
    {
        free(job);
        return NULL;
    }
    job->thread_count = thread_count;
    job->threads = calloc(job->thread_count, sizeof(pthread_t));
    if (job->threads == NULL) {
        thread_pool_free(job);
        return NULL;
    }
    th_loc_info=malloc(sizeof(*th_loc_info));
    if(!th_loc_info)
    {
        thread_pool_free(job);
        return NULL;
    }
    th_loc_info->idx=-1;
    for (int i = 0; i < job->thread_count; ++i) {
        routine_args *args=malloc(sizeof(*args));
        if(!args)
        {
            thread_pool_release(job);
            return NULL;
        }
        args->idx=i;
        args->job=job;
        if (pthread_create(&job->threads[i], NULL, thread_routine, args) != 0) {
            thread_pool_release(job);
            return NULL;
        }
        job->thread_start++;
    }
    return job;
}

void thread_pool_release(thread_pool *job)
{
    if(job->shutdown) return;
    job->shutdown = 1;
    for (int i = 0; i < job->thread_start; ++i) {
        pthread_join(job->threads[i],NULL);
    }
    read_work(job);
    //printf("thread_pool_task_cnt:%d\n",thread_pool_task_cnt(job));
    if(th_loc_info) free(th_loc_info);
    thread_pool_free(job);
}
