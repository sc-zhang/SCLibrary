#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"

void task_func(void* args){
    int num = *(int*)args;
    for(int i=0; i<100; i++){
        printf("number = %d, loop = %d\n", num, i);
    }
}

int main()
{
    ThreadPool* pool = init(10, 20, 100);
    for(int i=0; i<100; i++){
        int* num = (int*)malloc(sizeof(int));
        *num = i+100;
        task_add(pool, task_func, num);
    }
    destroy(pool);
    return 0;
}
