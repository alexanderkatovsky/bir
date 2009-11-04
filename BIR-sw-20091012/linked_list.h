#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "assoc_array.h"


struct linked_list
{
    void (* delete)(void *);
    struct assoc_array * array;
    int head;
};


struct linked_list_node
{
    int i;
    void * data;
    struct linked_list * list;
};

void linked_list_delete_list(struct linked_list * list, void (* delete)(void *));
struct linked_list * linked_list_create();
void linked_list_add(struct linked_list * list, void * data);
void linked_list_walk_list(struct linked_list * list, int (* fn)(void *, void *), void * user_data);
int linked_list_empty(struct linked_list * list);

#endif

