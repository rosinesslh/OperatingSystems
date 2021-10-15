#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

#define TEST_ASSERT(assert)			\
do {						\
	printf("ASSERT: " #assert " ... ");	\
	if (assert) {				\
		printf("PASS\n");		\
	} else	{				\
		printf("FAIL\n");		\
		exit(1);			\
	}					\
} while(0)					\

static int inc_item(queue_t q, void *data, void *arg)
{
	int *a = (int*)data;
	int inc = (int)(long)arg;

	if (*a == 42)
		queue_delete(q, data);
	else
		*a += inc;
	return 0;
}

static int delete_item(queue_t q, void *data, void *arg)
{
	int *a = (int*)data;
	int match = (int)(long)arg;

	if (*a == match)
		queue_delete(q, data);
	else
		return -1;
	return 0;
}

/* Callback function that finds a certain item according to its value */
static int find_item(queue_t q, void *data, void *arg)
{
	int *a = (int*)data;
	int match = (int)(long)arg;
	(void)q; //unused

	if (*a == match)
		return 1;
	return 0;
}

void test_iterator(void)
{
	queue_t q;
	int data[] = {1, 2, 3, 4, 5, 42, 6, 7, 8, 9};
	size_t i;
	int *ptr;

	/* Initialize the queue and enqueue items */
	q = queue_create();
	for (i = 0; i < sizeof(data) / sizeof(data[0]); i++)
		queue_enqueue(q, &data[i]);

	/* Add value '1' to every item of the queue, delete item '42' */
	fprintf(stderr, "*** TEST increment ***\n");
	queue_iterate(q, inc_item, (void*)1, NULL);
	TEST_ASSERT(data[0] == 2);
	TEST_ASSERT(queue_length(q) == 9);

	/* Find and get the item which is equal to value '5' */
	fprintf(stderr, "*** TEST find value ***\n");
	ptr = NULL;     // result pointer *must* be reset first
	queue_iterate(q, find_item, (void*)5, (void**)&ptr);
	TEST_ASSERT(ptr != NULL);
	TEST_ASSERT(*ptr == 5);
	TEST_ASSERT(ptr == &data[3]);

	/* Test dequeue the whole queue & destroy it */
	ptr = NULL;
	fprintf(stderr, "*** TEST dequeue general and destroy ***\n");
	int qleng= queue_length(q);
	for(int i = 0; i < qleng ; i++){
	queue_dequeue(q,(void**)&ptr);
	}
	TEST_ASSERT(queue_length(q) == 0);
	TEST_ASSERT(queue_destroy(q) == 0);
}

/* Create */
void test_create(void)
{
	fprintf(stderr, "*** TEST create ***\n");
	TEST_ASSERT(queue_create() != NULL);
}

/* Enqueue/Dequeue simple */
void test_queue_simple(void)
{
	int data = 3, *ptr = NULL;
	queue_t q;

	fprintf(stderr, "*** TEST enqueue simple ***\n");

	q = queue_create();
	TEST_ASSERT(queue_length(q) == 0);
	queue_enqueue(q, &data);
	TEST_ASSERT(queue_length(q) == 1);

	/* test cant find data*/
	fprintf(stderr, "*** TEST can't find value ***\n");
	queue_iterate(q, find_item, (void*)1, (void**)&ptr);
	TEST_ASSERT(ptr == NULL);

	/*test find data then delete*/
	fprintf(stderr, "*** TEST dequeue simple and edge cases ***\n");
	ptr = NULL;
	queue_dequeue(q, (void**)&ptr);
	TEST_ASSERT(ptr == &data);
	TEST_ASSERT(queue_length(q) == 0);
	TEST_ASSERT(queue_destroy(q) == 0);
}

/* 3 item queue delete first, last & middle & edge cases*/
void test_three_item(void){
	int data[] = {0,1,2};
	int data_size = 3;
	queue_t q;

	fprintf(stderr, "*** TEST queue with three items ***\n");
	q = queue_create();
	for (int i = 0; i < data_size; i++)
		queue_enqueue(q, &data[i]);

	TEST_ASSERT(queue_length(q) == 3);

	fprintf(stderr, "*** TEST delete middle item ***\n");
	TEST_ASSERT(queue_iterate(q, delete_item, (void*)1, NULL) == 0);
	TEST_ASSERT(queue_length(q) == 2);

	fprintf(stderr, "*** TEST delete first item ***\n");
	TEST_ASSERT(queue_iterate(q, delete_item, (void*)0, NULL) == 0);
	TEST_ASSERT(queue_length(q) == 1);


	fprintf(stderr, "*** TEST delete last item ***\n");
	TEST_ASSERT(queue_iterate(q, delete_item, (void*)2, NULL)==0);
	TEST_ASSERT(queue_length(q) == 0);

	fprintf(stderr, "*** TEST enqueue a null ptr ***\n");
	TEST_ASSERT(queue_enqueue(q, NULL) == -1);

	fprintf(stderr, "*** TEST dequeue an empty queue ***\n");
	TEST_ASSERT(queue_dequeue(q, NULL) == -1);
}


int main(void)
{
	test_create();
	test_queue_simple();
	test_iterator();
	test_three_item();
	fprintf(stderr, "*** Test complete. ***\n");
	return 0;
}
