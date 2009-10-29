#include "fifo.h"
#include <stdlib.h>

struct fifo * fifo_create_thread_safe_fifo()
{
    struct fifo * list = (struct fifo *)malloc(sizeof(struct fifo));
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    pthread_mutex_init(&list->mutex,NULL);
    return list;
}

void * fifo_pop(struct fifo * list)
{
    void * ret = 0;
    struct FifoNode * node;
    pthread_mutex_lock(&list->mutex);
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
    pthread_mutex_unlock(&list->mutex);
    return ret;
}

void fifo_push(struct fifo * list, void * data)
{
    struct FifoNode * node;
    pthread_mutex_lock(&list->mutex);
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
    pthread_mutex_unlock(&list->mutex);
}

int fifo_length(struct fifo * list)
{
    int ret;
    pthread_mutex_lock(&list->mutex);
    ret = list->length;
    pthread_mutex_unlock(&list->mutex);
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

