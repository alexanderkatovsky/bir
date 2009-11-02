#include "mutex.h"
#include "common.h"

struct sr_mutex * mutex_create()
{
    NEW_STRUCT(ret,sr_mutex);
    pthread_mutexattr_init(&ret->mta);
    pthread_mutexattr_settype(&ret->mta,PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&ret->mutex, &ret->mta);
    return ret;
}

void mutex_destroy(struct sr_mutex * mutex)
{
    pthread_mutex_destroy(&mutex->mutex);
    pthread_mutexattr_destroy(&mutex->mta);
    free(mutex);
}

void mutex_lock(struct sr_mutex * mutex)
{
    pthread_mutex_lock(&mutex->mutex);
}

void mutex_unlock(struct sr_mutex * mutex)
{
    pthread_mutex_unlock(&mutex->mutex);
}
