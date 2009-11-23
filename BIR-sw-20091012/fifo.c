#include "fifo.h"
#include <stdlib.h>

struct fifo * fifo_create()
{
    struct fifo * list = (struct fifo *)malloc(sizeof(struct fifo));
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    return list;
}

void fifo_destroy(struct fifo * list)
{
    free(list);
}

void * fifo_pop(struct fifo * list)
{
    void * ret = 0;
    struct FifoNode * node;
    if(list->length > 0)
    {
        node = list->tail;
        list->length -= 1;
        ret = list->tail->data;
        list->tail = node->next;
        free(node);
    }
    if(list->length == 0)
    {
        list->head = NULL;
    }
    return ret;
}

void fifo_push(struct fifo * list, void * data)
{
    struct FifoNode * node;
    node = (struct FifoNode *)malloc(sizeof(struct FifoNode));
    node->data = data;
    node->next = NULL;
    if(list->head)
    {
        list->head->next = node;
    }
    if(list->tail == NULL)
    {
        list->tail = node;
    }
    list->head = node;
    list->length += 1;
}

int fifo_length(struct fifo * list)
{
    int ret;
    ret = list->length;
    return ret;
}

void fifo_delete(struct fifo * list,void (* delete)(void *))
{
    struct FifoNode * node = list->head;
    struct FifoNode * next;
    while(node)
    {
        next = node->next;
        if(delete)
        {
            delete(node->data);
        }
        free(node);
        node = next;
    }
    free(list);
}

int fifo_empty(struct fifo * list)
{
    return (list->length == 0);
}
