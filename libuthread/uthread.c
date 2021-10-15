#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "private.h"
#include "uthread.h"
#include "queue.h"

struct uthread_data {
	uthread_t tid;
	uthread_ctx_t *context;
	void *stk_ptr; //pointer to top of stack
	int thd_joined; //thread to join with
	int ev; //return value
};

static int num_threads = 0;
static int preempt_required;
static struct uthread_data *running; //currently running thread
static queue_t readyQ; //ready threads
static queue_t blockedQ; //blocked threads
static queue_t deadQ; //zombie threads

//search method to be iterated. used to find data pointer with given tid
static int find_thd_data(queue_t q, void *data, void *arg)
{
	struct uthread_data *tdata = (struct uthread_data*) data;
	uthread_t tidmatch = *((uthread_t*) arg);
	(void)q; //unused

	if (tdata->tid == tidmatch) return 1;

	return 0;
}

int uthread_start(int preempt)
{
	preempt_required = preempt;
	//allocate memory
	readyQ = queue_create();
	blockedQ = queue_create();
	deadQ = queue_create();
	running = (struct uthread_data*) malloc(sizeof(struct uthread_data));
	uthread_ctx_t *ctx = (uthread_ctx_t*) malloc(sizeof(uthread_ctx_t));
	//check for failed malloc
	if (!(readyQ && blockedQ && deadQ && running && ctx)) {
		if (readyQ) free(readyQ);
		if (blockedQ) free(blockedQ);
		if (deadQ) free(deadQ);
		if (running) free(running);
		if (ctx) free(ctx);
		return -1;
	}
	//set properties of initial thread
	running->tid = 0;
	running->context = ctx;
	running->stk_ptr = NULL;
	running->thd_joined = -1;
	num_threads++;

	if (preempt_required == 1) preempt_start();

	return 0;
}

int uthread_stop(void)
{
	if (running && queue_length(readyQ) == 0 && queue_length(blockedQ) == 0 && queue_length(deadQ) == 0) {
		if (preempt_required) preempt_stop();
		//free memory
		queue_destroy(readyQ);
		queue_destroy(blockedQ);
		queue_destroy(deadQ);
		free(running->context);
		free(running);
		running = NULL;
		num_threads = 0;
		return 0;
	}
	return -1;
}

int uthread_create(uthread_func_t func)
{
	struct uthread_data *thd = (struct uthread_data*) malloc(sizeof(struct uthread_data));
	uthread_ctx_t *ctx = (uthread_ctx_t*) malloc(sizeof(uthread_ctx_t));
	void *stk = uthread_ctx_alloc_stack();

	preempt_disable();
	//classify new thread as ready. critical zone where queue is modified
	if (!(num_threads <= USHRT_MAX && thd && ctx && stk 
	&& uthread_ctx_init(ctx, stk, func) == 0 
	&& queue_enqueue(readyQ, (void*) thd) == 0)) {
		preempt_enable();
		if (thd) free(thd);
		if (ctx) free(ctx);
		if (stk) uthread_ctx_destroy_stack(stk);
		return -1;
	}
	thd->tid = num_threads;
	thd->context = ctx;
	thd->stk_ptr = stk;
	thd->thd_joined = -1;

	num_threads++;

	preempt_enable();

	return thd->tid;
}

void uthread_yield(void)
{
	preempt_disable();
	if(queue_length(readyQ) > 0) { //check if there is sth in readyQ
		//replace current thread with another ready thread
		uthread_ctx_t *prev = running->context;
		queue_enqueue(readyQ, (void*) running);
		queue_dequeue(readyQ, (void**) &running);
		uthread_ctx_t *next = running->context;
		uthread_ctx_switch(prev, next);
	}
	preempt_enable();
}

uthread_t uthread_self(void)
{
	return (running -> tid);
}

void uthread_exit(int retval)
{
	//running is constant with respect to context as long as it is not reassigned
	running->ev = retval;
	uthread_t tjoin = running->thd_joined;
	uthread_ctx_t *prev = running->context;
	preempt_disable(); //entering critical zone where queues are modified
	//classify thread as dead
	queue_enqueue(deadQ, (void*) running);
	//unblock thread to join with
	struct uthread_data *td = NULL;
	queue_iterate(blockedQ, (queue_func_t)find_thd_data, (void*)&tjoin, (void**)&td);
	if (td) {
		queue_delete(blockedQ, (void*) td);
		queue_enqueue(readyQ, (void*) td);
	}
	//assign new running thread
	queue_dequeue(readyQ, (void**) &running);
	uthread_ctx_t *next = running->context;
	uthread_ctx_switch(prev, next);
	preempt_enable();
}

int uthread_join(uthread_t tid, int *retval)
{
	if (tid == 0 || tid == running->tid) return -1;

 	//check if tid dead already
	struct uthread_data *target = NULL;
	preempt_disable();
	queue_iterate(deadQ, (queue_func_t)find_thd_data, (void*)&tid, (void**)&target);
	if (target){	
		//T2 dead
		if (target->thd_joined != -1) return -1;
		if (retval) *retval = target->ev;
		queue_delete(deadQ, target);
		preempt_enable();
		uthread_ctx_destroy_stack(target->stk_ptr);
		free(target->context);
		free(target);
		return 0;
	}
	//search for thread
	queue_iterate(readyQ, (queue_func_t)find_thd_data, (void*)&tid, (void**)&target);
	queue_iterate(blockedQ, (queue_func_t)find_thd_data, (void*)&tid, (void**)&target);
	if (!target) return -1;

	//T2 is in readyQ or blockedQ
	target->thd_joined = running->tid;

	uthread_ctx_t *prev = running->context;
	queue_enqueue(blockedQ, (void*) running);
	queue_dequeue(readyQ, (void**) &running);
	uthread_ctx_t *next = running->context;
	uthread_ctx_switch(prev, next);

	//T2 is in deadQ
	if (retval) *retval = target->ev;
	queue_delete(deadQ, target);
	
	preempt_enable();
	//free memory
	uthread_ctx_destroy_stack(target->stk_ptr);
	free(target->context);
	free(target);

	return 0;
}

