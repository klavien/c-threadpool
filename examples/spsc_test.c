#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include "spsc.h"

struct args{
	int idx;
	char msg[16];
};
void task(spsc_watcher* arg){
	struct args *a=arg->data;
	//if(a->idx==1000000) 
	{
		printf("%s,t_idx:%d\n",a->msg,arg->th_info->idx);
	}
	free(a);
}
void *producer(void* data)
{
	spsc *thpool=(spsc *)data;
	for (int i=0; i<1000000; i++){
		struct args *arg=malloc(sizeof(struct args));
		if(!arg)
		{
			puts("malloc error!");
			return NULL;
		}
		arg->idx=i+1;
		snprintf(arg->msg,sizeof(arg->msg),"%d",i+1);
		spsc_add(thpool, task, (void*)arg);
	};
}
int main(){
	spsc *thpool = spsc_create(1024);

	for (int i=0; i<10; i++){
		producer(thpool);
	};
	pthread_t threads[10];
	for (int i=0; i<10; i++){
		if (pthread_create(&threads[i], NULL, producer, thpool) != 0) {
			puts("pthread_create error!");
            return -1;
        }
	};
	for (int i = 0; i < 10; ++i) {
        if(pthread_join(threads[i],NULL)!=0)
		{
			puts("pthread_join error!");
			continue;
		}
    }
	printf("spsc_task_cnt:%d\n",spsc_task_cnt(thpool));
	
	spsc_release(thpool);
	return 0;
}