#include "linked_list.h"
#include "common.h"

void __linked_list_delete_list(void * data)
{
    struct linked_list_node * node = (struct linked_list_node *)data;
    node->list->delete(node->data);
    free(node);
}

void linked_list_delete_list(struct linked_list * list, void (* delete)(void *))
{
    list->delete = delete;
    assoc_array_delete_array(list->array,__linked_list_delete_list);
    free(list);
}

void * __linked_list_get_key(void * data)
{
    return &(((struct linked_list_node *)data)->i);
}

struct linked_list * linked_list_create()
{
    NEW_STRUCT(ret,linked_list);
    ret->array = assoc_array_create(__linked_list_get_key,assoc_array_key_comp_int);
    ret->head = 0;
    return ret;
}

void linked_list_add(struct linked_list * list, void * data)
{
    NEW_STRUCT(node,linked_list_node);
    node->data = data;
    node->list = list;
    node->i = list->head;
    if(assoc_array_read(list->array,node) == NULL)
    {
        list->head += 1;
        assoc_array_insert(list->array,node);
    }
}

void linked_list_walk_list(struct linked_list * list, int (* fn)(void *, void *), void * user_data)
{
    int i = 0;
    for(;i < list->head; i++)
    {
        if(fn(((struct linked_list_node *)assoc_array_read(list->array,&i))->data,user_data))
        {
            break;
        }
    }
}

int linked_list_empty(struct linked_list * list)
{
    return (list->head == 0);
}
