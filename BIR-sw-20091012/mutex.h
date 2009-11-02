#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

struct sr_mutex
{
    pthread_mutex_t mutex;
    pthread_mutexattr_t mta;
};

struct sr_mutex * mutex_create();
void mutex_destroy(struct sr_mutex * mutex);
void mutex_lock(struct sr_mutex * mutex);
void mutex_unlock(struct sr_mutex * mutex);


#endif
