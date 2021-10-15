#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "uthread.h"

int thread(void) {
	while (1) {
		printf("thread\n");
	}
}

int test1(void) {

	uthread_start(1);
	uthread_create(thread);
	uthread_yield();
	printf("preemption has occurred\n");
	return 0;
}

int main(void){
	test1();
	return 0;

}
