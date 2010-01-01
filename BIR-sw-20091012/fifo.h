#ifndef FIFO_H
#define FIFO_H

#include <pthread.h>


/*
 *Provides a thread safe FIFO queue
 * */

struct FifoNode
{
    struct FifoNode * next;
    void * data;
};

struct fifo
{
    struct FifoNode * head;
    struct FifoNode * tail;

    int length;
};

struct fifo * fifo_create();
void fifo_destroy(struct fifo *);
void * fifo_pop(struct fifo * list);
void fifo_push(struct fifo * list, void * data);
int fifo_length(struct fifo * list);
void fifo_delete(struct fifo * list,void (* _delete)(void *));
int fifo_empty(struct fifo * list);

#endif
