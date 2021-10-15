# libuthread: User-level thread library

## Summary

This project, `libuthread`, is a basic user-level thread library for Linux
similar to the existing user-level thread library `pthread`. 

`libuthread` provides interfaces for applications to create independent
threads and allows applications to run multiple threads concurrently in
a round-robin fashion with a preemptive scheduler. It also provides a 
synchronization mechanism for threads to join other threads and collect
their return value.

## Implementation

The implementation of `libuthread` library mainly consists of three 
parts:
1. queue API provides a basic structure that holds information about 
threads.
2. preemption API can force the current running thread to yield to 
next ready thread.
3. uthread API which provides functions for users to start, stop 
multithreading; create, exit a thread; yield to next thread as well as 
join other threads.

### Queue API

In order to store threads in queues, we implemented a First-in-First-Out 
queue using void pointer so it can support and store more data types in 
the queue. This queue structure is able to create, destroy, enqueue and 
dequeue with another feature `queue_iterate` which provides a way to 
call a custom function on every item currently in the queue.

### Preemption API

The preemptive scheduling of the `uthread` library is achieved by using 
a signal handler modifies alarm signal `SIGVTALRM` and a timer which 
fires an alarm through `SIGVTALRM` signal.

After users start multithreading with preemption, the alarm will fire an 
alarm signal 100 times per second through `SIGVTALRM` and with each 
timer interrupt, the current running thread will yield to next ready thread.
We use `itimerval` structure for the timer and set each interval between 
alarm signals as 10000 microseconds and we modify signal handler of 
`SIGVTALRM` to be `timer_handler` which calls `uthread_yield` and will 
force the current running thread to yield to the next thread in the ready 
queue.

The `preempt_disable` function can temporarily block the `SIGVTALRM` 
signal then restores using `preempt_enable`. These two functions can
protect threads from timer interrupts when entering critical sections.

### Uthread API

The implementation of `uthread` uses queue as the basic structure. We 
created a structure `uthread_data` to store information about a thread 
such as TID, context, return value. We also set up 3 queues `blockQ`, 
`readyQ`, `deadQ` to hold `uthread_data` of threads in different states. 
Only threads in `readyQ` will be executed while threads in `blockQ` is 
blocked. After threads finished execution, they will be put into `deadQ` 
and waiting to be collected by another thread that joins them. There is 
a helper function `find_the_data` that can be used with `queue_iterate` 
to look for a specific thread in a certain queue.

Users can choose if they want threads to run concurrently with 
preemption when calling `uthread_start` to start multithreading. If 
preemption is turned on, the multithread execution will run in a 
round-robin way with a preemptive scheduler that forces threads to yield 
after a certain time. If preemption is turned off, threads can also yield 
to the next ready thread by calling `uthread_yield` voluntarily. When all 
user threads are finished, the main thread needs to call `uthread_stop` 
and stops the multithreading library, destroy created queues then free all 
resources.

Running thread can call `uthread_create` to create more threads, this 
function will return the TID of the new thread. Those new threads are 
enqueued into `readyQ` after creation. Running thread can also call 
`uthread_exit()` to finish up its execution then enqueue to `deadQ` and 
let the thread who joins it collects the return value then switch context to 
next ready queue.

`uthread_join` function is for a thread to join its child thread, 
collect its return value and free allocated resources. The target thread 
TID get passed to the function by parameter and we first look for it in 
`deadQ` with the helper function to check if the target thread is already 
completed. If the target thread is in `blockQ` or `readyQ`, the calling 
thread will be block by enqueuing it into `blockQ` until the target thread 
dies. `uthread_join` returns -1 if the target TID is 0 or is the TID of 
itself.
 
## Testing

The Testing for this project also involves with the previous three APIs.
1. Queue Testing
2. Preemption Testing
3. Uthread Testing

### Queue Testing

We use unit testing to fully test queue implementation. To ensure it is 
resistant and match the specifications, we used assertion on all 
possible scenarios including edge cases like deleting first/last element 
and possible mistakes like enqueue a `NULL` pointer or dequeue an empty 
queue.

### Preemption Testing

In this tester, we implemented a wrapper function `test1` which starts 
multithreading execution and creates a new thread named `thread` then yields to it. 
`thread` has an infinite while-loop that keeps printing out `thread`. Without 
preemption, the infinite while-loop will continue without an end whereas with 
preemption, when `thread` is forced to yield back to `test1` by the preemptive 
scheduler, `test1` will then print `preemption has occurred` and return.

If `test1` calls `uthread_start(1)` with parameter 1, then preemption is enabled.
From the output, we can observe the program prints `thread` for some time, then 
prints `preemption has occurred` and ends after that.

If `test1` calls `uthread_start(0)` with parameter 0, then preemption is 
disabled. Because `thread` does not call `uthread_yield`, it keeps running 
without yielding. Therefore the program stucks within the infinite while-loop and 
keeps printing `thread`.

### Uthread Testing

We test our basic implementation on uthread API by editing `uthread_hello.c` 
and `uthread_yield.c` with proper `uthread_join` function. In addition to that,
we implemented `uthread_test.c` to expand testing on the return value and 
`uthread_exit` function which is not tested before.

`uthread_test.c` creates three more threads. `main` calls `uthread_start(1)` to
enable preemption. We let threads print out their own TID by calling 
`uthread_self`. Then each thread will yield to next ready thread using 
`uthread_yield`. `uthread_exit` is tested in `thread3`, if it works, remaining
 instructions in `thread3` will not be executed. After parent threads join 
 child thread using `uthread_join`, it will then print out child thread 
 return value.

## Reference
https://
stackoverflowcomquestions48064940understanding-struct-itimerval-field-tv-usec
