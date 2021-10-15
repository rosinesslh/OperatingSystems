#include <stdio.h>
#include <stdlib.h>
#include "uthread.h"

static int retval;

int thread3(void)
{
        for (int i = 0; i < 20; i++){
            printf("I'm 3 and printing 0 - 19 : %d\n", i);
        }
        uthread_yield();
        printf("I'm thread%d\n", uthread_self());

        uthread_exit(retval);
        //should not be pringting following
        for (int j = 20; j < 40; j++){
            printf("I'm 3 and printing 20 - 39 : %d\n", j);
        }
        return 0;
}

int thread2(void)
{
        uthread_t tid = uthread_create(thread3);
        uthread_yield();
        printf("I'm thread%d\n", uthread_self());
        uthread_join(tid, &retval);
        printf("thread 3 return value is: %d\n", retval);
        return 0;
}

int thread1(void)
{
        uthread_t tid = uthread_create(thread2);
        uthread_yield();
        printf("I'm thread%d\n", uthread_self());
        uthread_join(tid, &retval);
        printf("thread 2 return value is: %d\n", retval);
        return 0;
}

int main(void)
{
        uthread_start(1);
        uthread_join(uthread_create(thread1), &retval);
        printf("thread 1 return value is: %d\n", retval);
        uthread_stop();

        return 0;
}