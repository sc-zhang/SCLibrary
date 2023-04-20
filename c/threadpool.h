#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MODCOUNT 2

typedef struct Task{
    void (*function)(void* args);
    void* args;
} Task;

typedef struct ThreadPool{
    Task* task_queue;
    int queue_capacity;
    int queue_size;
    int queue_front;
    int queue_rear;

    pthread_t manager_id;
    pthread_t *thread_ids;
    int min_num;
    int max_num;
    int busy_num;
    int live_num;
    int exit_num;
    pthread_mutex_t mutex_pool;
    pthread_mutex_t mutex_busy;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    int shutdown;
}ThreadPool;

ThreadPool* init(int min_num, int max_num, int size);
void *manager(void* args);
void* worker(void* args);
void task_add(ThreadPool* pool, void(*func)(void*), void* args);
int thread_pool_busy_num(ThreadPool* pool);
int thread_pool_live_num(ThreadPool* pool);
int destroy(ThreadPool* pool);
void thread_exit(ThreadPool* pool);

ThreadPool* init(int min_num, int max_num, int size){
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
        pool->thread_ids = NULL;
        pool->task_queue = NULL;
        if(!pool){
            printf("malloc threadpool failed\n");
            break;
        }
        pool->thread_ids = (pthread_t*)malloc(sizeof(pthread_t)*max_num);
        if(!pool->thread_ids){
            printf("malloc thread ids failed\n");
            break;
        }
        memset(pool->thread_ids, 0, sizeof(pthread_t)*max_num);
        pool->min_num = min_num;
        pool->max_num = max_num;
        pool->busy_num = 0;
        pool->live_num = min_num;
        pool->exit_num = 0;
        if(pthread_mutex_init(&pool->mutex_pool, NULL) ||
                pthread_mutex_init(&pool->mutex_busy, NULL) ||
                pthread_cond_init(&pool->not_full, NULL) ||
                pthread_cond_init(&pool->not_empty, NULL)){
            printf("mutex or condition init failed\n");
            break;
        }
        pool->task_queue = (Task*)malloc(sizeof(Task)*size);
        if(!pool->task_queue){
            printf("malloc task queue failed\n");
            break;
        }
        pool->queue_capacity = size;
        pool->queue_size = 0;
        pool->queue_front = 0;
        pool->queue_rear = 0;
        pool->shutdown = 0;
        pthread_create(&pool->manager_id, NULL, manager, pool);
        for(int i=0; i<min_num; i++){
            pthread_create(&pool->thread_ids[i], NULL, worker, pool);
        }
        return pool;
    }while(0);
    if(pool){
        if(pool->thread_ids) free(pool->thread_ids);
        if(pool->task_queue) free(pool->task_queue);
        free(pool);
    }
    return NULL;
}

void* worker(void* args){
    ThreadPool* pool = (ThreadPool*)args;
    while(1){
        pthread_mutex_lock(&pool->mutex_pool);
        while(pool->queue_size==0 && !pool->shutdown){
            pthread_cond_wait(&pool->not_empty, &pool->mutex_pool);
            if(pool->exit_num>0){
                pool->exit_num--;
                if(pool->live_num>pool->min_num){
                    pool->live_num--;
                    pthread_mutex_unlock(&pool->mutex_pool);
                    thread_exit(pool);
                }
            }
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutex_pool);
            thread_exit(pool);
        }
        Task task;
        task.function = pool->task_queue[pool->queue_front].function;
        task.args = pool->task_queue[pool->queue_front].args;
        pool->queue_front = (pool->queue_front+1) % pool->queue_capacity;
        pool->queue_size--;
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->mutex_pool);

        pthread_mutex_lock(&pool->mutex_busy);
        pool->busy_num++;
        pthread_mutex_unlock(&pool->mutex_busy);
        task.function(task.args);
        free(task.args);
        task.args = NULL;

        pthread_mutex_lock(&pool->mutex_busy);
        pool->busy_num--;
        pthread_mutex_unlock(&pool->mutex_busy);
    }
    return NULL;
}

void *manager(void* args){
    ThreadPool* pool = (ThreadPool*)args;
    while(!pool->shutdown){
        sleep(3);
        pthread_mutex_lock(&pool->mutex_pool);
        int queue_size = pool->queue_size;
        int live_num = pool->live_num;
        int busy_num = pool->busy_num;
        pthread_mutex_unlock(&pool->mutex_pool);

        if(queue_size > live_num && live_num < pool->max_num){
            pthread_mutex_lock(&pool->mutex_pool);
            int counter = 0;
            for(int i=0; i<pool->max_num&&counter<MODCOUNT&&pool->live_num<pool->max_num; ++i){
                if(pool->thread_ids[i] == 0){
                    pthread_create(&pool->thread_ids[i], NULL, worker, pool);
                    counter++;
                    pool->live_num++;
                }
            }
            pthread_mutex_unlock(&pool->mutex_pool);
        }

        if(busy_num*2<live_num && live_num>pool->min_num){
            pthread_mutex_lock(&pool->mutex_pool);
            pool->exit_num = MODCOUNT;
            pthread_mutex_unlock(&pool->mutex_pool);
            for(int i=0; i<pool->exit_num; i++){
                pthread_cond_signal(&pool->not_empty);
            }
        }
    }
    return NULL;
}


void thread_exit(ThreadPool* pool){
    pthread_t tid = pthread_self();
    for(int i=0; i<pool->max_num; i++){
        if(pool->thread_ids[i] == tid){
            pool->thread_ids[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}

void task_add(ThreadPool* pool, void(*func)(void*), void* args){
    pthread_mutex_lock(&pool->mutex_pool);
    while(pool->queue_size==pool->queue_capacity && !pool->shutdown){
        pthread_cond_wait(&pool->not_full, &pool->mutex_pool);
    }
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutex_pool);
        return;
    }
    pool->task_queue[pool->queue_rear].function = func;
    pool->task_queue[pool->queue_rear].args = args;
    pool->queue_rear = (pool->queue_rear+1) % pool->queue_capacity;
    pool->queue_size++;

    pthread_cond_signal(&pool->not_empty);

    pthread_mutex_unlock(&pool->mutex_pool);
}

int thread_pool_busy_num(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutex_busy);
    int busy_num = pool->busy_num;
    pthread_mutex_unlock(&pool->mutex_busy);
    return busy_num;
}

int thread_pool_live_num(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutex_pool);
    int live_num = pool->live_num;
    pthread_mutex_unlock(&pool->mutex_pool);
    return live_num;
}

int destroy(ThreadPool* pool){
    if(!pool){
        return -1;
    }
    while(pool->queue_size){
        sleep(1);
    }
    pool->shutdown = 1;
    pthread_join(pool->manager_id, NULL);

    for(int i=0; i<pool->live_num; i++){
        pthread_cond_signal(&pool->not_empty);
    }
    int wait_exit = 1;
    while(wait_exit){
        sleep(1);
        wait_exit = 0;
        for(int i=0; i<pool->max_num; i++){
            if(pool->thread_ids[i] != 0){
                wait_exit = 1;
                break;
            }
        }
    }
    if(pool->thread_ids){
        free(pool->thread_ids);
        pool->thread_ids = NULL;
    }

    pthread_mutex_destroy(&pool->mutex_pool);
    pthread_mutex_destroy(&pool->mutex_busy);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    free(pool);
    pool = NULL;

    return 0;
}

#endif // THREADPOOL_H
