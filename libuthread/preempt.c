#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "private.h"
#include "uthread.h"

/* reference https://stackoverflow.com/questions/48064940/understanding-struct-itimerval-field-tv-usec*/

/*
 * Frequency of preemption
 * 100Hz is 100 times per second
 * Every 10000 microseconds between each fire
 */
#define HZ 100
#define INTERVAL 10000 

static struct sigaction sa;
static struct sigaction oldsa;
static struct itimerval old;
static struct itimerval new;
static int on = 0;


static void timer_handler(void)
{
	uthread_yield();
}

void preempt_start(void)
{
	on = 1;
	sa.sa_handler = (__sighandler_t)timer_handler;
	sigemptyset (&sa.sa_mask);
	sigemptyset (&oldsa.sa_mask);
	sigaddset(&sa.sa_mask, SIGVTALRM);
	preempt_enable();
	sigaction(SIGVTALRM, &sa, &oldsa);
	//period between each succesive timer is 10000 micro sec
	new.it_interval.tv_sec = 0;
	new.it_interval.tv_usec = INTERVAL;
	//period between now and first timer is 10000 micro sec
	new.it_value.tv_sec = 0;
	new.it_value.tv_usec = INTERVAL;
	setitimer (ITIMER_VIRTUAL, &new, &old);
}

void preempt_stop(void)
{
	preempt_disable();
	sigaction(SIGVTALRM, &oldsa, NULL);
	setitimer (ITIMER_VIRTUAL, &old, NULL);
	on = 0;
}

/*unblock the signal with preemption*/
void preempt_enable(void)
{
	if (!on) return;
	sigset_t *ptr = NULL;
	sigemptyset (&sa.sa_mask);
	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, ptr);
}

/*block the signal no preemption*/
void preempt_disable(void)
{
	if (!on) return;
	sigset_t *ptr = NULL;
	sigemptyset (&sa.sa_mask);
	sigprocmask(SIG_BLOCK, &sa.sa_mask, ptr);
}

