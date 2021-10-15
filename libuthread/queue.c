#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

struct node {
	void* data;
	struct node* next;
};

struct queue {
	int length;
	struct node *front;
	struct node *rear;
};

queue_t queue_create(void)
{
	//init queue
	queue_t newq = (queue_t) malloc(sizeof(struct queue));
	if (!newq) return NULL;
	newq->front = NULL;
	newq->rear = NULL;
	newq->length = 0;
	return newq;
}

int queue_destroy(queue_t queue)
{
	if (queue && !queue->front) {
		free(queue);
		return 0;
	}
	return -1;
}

int queue_enqueue(queue_t queue, void *data)
{
	if (!queue || !data) return -1;

	struct node *nn = (struct node*) malloc(sizeof(struct node));
	if (!nn) return -1;
	nn->data = data;
	nn->next = NULL;
	if (!queue->front) {
		queue->front = queue->rear = nn;
	} else {
		//add node to rear
		queue->rear->next = nn;
		queue->rear = nn;
	}
	queue->length++;

	return 0;
}

int queue_dequeue(queue_t queue, void **data)
{
	if (!data || !queue || !queue->front) return -1;

	void* d = NULL;
	if (queue->front) {
		//remove from front
		struct node *temp = queue->front;
		queue->front = queue->front->next;
		d = temp->data;
		free(temp);
		if (!queue->front) {
			queue->rear = NULL;
		}
	}
	*data = d;
	queue->length--;

	return 0;
}

int queue_delete(queue_t queue, void *data)
{
	if (!queue || !data) return -1;
	//node* variables used for iteration
	struct node dummy = {NULL, queue->front};
	struct node *prev = &dummy;
	struct node *curr = queue->front;
	//iterate until data found or end reached
	while (curr && curr->data != data) {
		curr = curr->next;
		prev = prev->next;
	}
	if (!curr) return -1;
	//reassign pointers
	if (curr == queue->rear && curr == queue->front) {
		queue->rear = queue->front = NULL;
	}
	else if (curr == queue->rear) {
		queue->rear = prev;
	}
	else if (curr == queue->front) {
		queue->front = curr->next;
	}

	prev->next = curr->next;
	free(curr);
	queue->length--;
	
	return 0;
}

int queue_iterate(queue_t queue, queue_func_t func, void *arg, void **data)
{
	if (!queue || !func) return -1;

	struct node *curr = queue->front;
	struct node *next = NULL;
	while (curr) {
		// store curr->next in case curr is deleted by func
		next = curr->next;
		if (func(queue, curr->data, arg) == 1) {
			if (data) *data = curr->data;
			break;
		}
		curr = next;
	}

	return 0;
}

int queue_length(queue_t queue)
{
	if (!queue) return -1;
	return queue->length;
}

