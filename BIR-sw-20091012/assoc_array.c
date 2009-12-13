#include "assoc_array.h"
#include <stdlib.h>
#include <string.h>


/*
 *Implementation of AA Tree for an associative array
 * */

struct AssocArrayNode * __skew(struct AssocArrayNode * node)
{
    struct AssocArrayNode * ret = NULL;
    if(node)
    {
        ret = node;
        if(node->left && node->left->level == node->level)
        {
            ret = node->left;
            node->left = ret->right;
            ret->right = node;
        }
    }
    return ret;
}

struct AssocArrayNode * __split(struct AssocArrayNode * node)
{
    struct AssocArrayNode * ret = NULL;
    if(node)
    {
        ret = node;
        if(node->right && node->right->right &&
           (node->level == node->right->right->level))
        {
            ret = node->right;
            node->right = ret->left;
            ret->left = node;
            ret->level += 1;
        }
    }
    return ret;
}

struct AssocArrayNode * __assoc_array_insert(struct assoc_array * array, struct AssocArrayNode * node, void * data)
{
    if(node == NULL)
    {
        node = (struct AssocArrayNode *) malloc(sizeof(struct AssocArrayNode));
        node->data = data;
        node->array = array;
        node->level = 1;
        node->right = NULL;
        node->left = NULL;
    }
    else
    {
        switch(array->cmp(array->get(data),array->get(node->data)))
        {
        case ASSOC_ARRAY_KEY_EQ:
            node->data = data;
            break;       
        case ASSOC_ARRAY_KEY_LT:
            node->left = __assoc_array_insert(array,node->left,data);
            break;
        case ASSOC_ARRAY_KEY_GT:
            node->right = __assoc_array_insert(array,node->right,data);
            break;
        }
        node = __skew(node);
        node = __split(node);
    }
    return node;
}

void assoc_array_insert(struct assoc_array * array, void * data)
{
    array->root = __assoc_array_insert(array,array->root,data);
}

#define __level(node) ((node) ? ((node)->level):0)

struct AssocArrayNode * __assoc_array_delete(struct assoc_array * array,
                                             struct AssocArrayNode * node, void ** data, void * key)
{
    struct AssocArrayNode * succ;
    
    if(node)
    {
        if(array->cmp(key,array->get(node->data)) == ASSOC_ARRAY_KEY_EQ)
        {
            *data = node->data;
            if(node->left && node->right)
            {
                succ = node->left;
                while(succ->right)
                {
                    succ = succ->right;
                }
                node->left = __assoc_array_delete(array,node->left,&node->data,array->get(succ->data));
            }
            else
            {
                if(node->right)
                {
                    *node = *(succ = node->right);
                    free(succ);
                }
                else if(node->left)
                {
                    *node = *(succ = node->left);
                    free(succ);
                }
                else
                {
                    free(node);
                    node = NULL;
                }
            }
        }
        else
        {
            if(array->cmp(key,array->get(node->data)) == ASSOC_ARRAY_KEY_GT)
            {
                node->right = __assoc_array_delete(array,node->right,data,key);
            }
            else
            {
                node->left = __assoc_array_delete(array,node->left,data,key);
            }
        }
        if(node)
        {
            if(   __level(node->left) < node->level - 1
                  || __level(node->right) < node->level - 1)
            {
                node->level -= 1;
                if(__level(node->right) > node->level)
                {
                    node->right->level = node->level;
                }

                node = __skew(node);
                node->right = __skew(node->right);
                node->right->right && (node->right->right = __skew(node->right->right));
                node = __split(node);
                node->right = __split(node->right);
            }
        }
    }

    return node;
}

void * assoc_array_delete(struct assoc_array * array, void * key)
{
    void * data = NULL;
    array->root = __assoc_array_delete(array,array->root,&data,key);
    return data;
}

void __assoc_array_delete_array(struct AssocArrayNode * node, void (* delete)(void *))
{
    if(node)
    {
        if(delete)
        {
            delete(node->data);
        }
        __assoc_array_delete_array(node->left,delete);
        __assoc_array_delete_array(node->right,delete);
    }
}

void assoc_array_delete_array(struct assoc_array * array, void (* delete)(void *))
{
    struct AssocArrayNode * root = array->root;
    __assoc_array_delete_array(root,delete);
    free(array);
}

void * __assoc_array_read(struct assoc_array * array, struct AssocArrayNode * node, void * key)
{
    void * ret = NULL;
    if(node)
    {
        switch(array->cmp(array->get(node->data),key))
        {
        case ASSOC_ARRAY_KEY_EQ:
            ret = node->data;
            break;       
        case ASSOC_ARRAY_KEY_LT:
            ret = __assoc_array_read(array, node->right, key);
            break;
        case ASSOC_ARRAY_KEY_GT:
            ret = __assoc_array_read(array, node->left, key);
            break;
        }
    }
    return ret;
}

void * assoc_array_read(struct assoc_array * array, void * key)
{
    struct AssocArrayNode * root = array->root;
    return __assoc_array_read(array,root,key);
}

void __assoc_array_walk(int * finished, struct AssocArrayNode * node,
                        int (* fn)(void *, void *), void * user_data)
{
    if(node)
    {
        if(*finished == 0)
        {
            __assoc_array_walk(finished,node->right,fn,user_data);
        }
        if(*finished == 0)
        {
            *finished = fn(node->data,user_data);
        }
        if(*finished == 0)
        {
            __assoc_array_walk(finished,node->left,fn,user_data);
        }
    }
}

void assoc_array_walk_array(struct assoc_array * array,
                            int (* fn)(void *, void *), void * user_data)
{
    struct AssocArrayNode * root = array->root;
    int finished = 0;
    __assoc_array_walk(&finished,root,fn,user_data);
}

struct assoc_array * assoc_array_create(assoc_array_key_getter get, assoc_array_key_comp cmp)
{
    struct assoc_array * array = (struct assoc_array *)malloc(sizeof(struct assoc_array));
    array->root = NULL;
    array->get = get;
    array->cmp = cmp;
    return array;
}


int assoc_array_key_comp_int(void * k1, void * k2)
{
    int ki1 = *((int*)k1);
    int ki2 = *((int*)k2);

    if( ki1 == ki2 )
    {
        return ASSOC_ARRAY_KEY_EQ;
    }
    else if( ki1 < ki2 )
    {
        return ASSOC_ARRAY_KEY_LT;
    }
    else
    {
        return ASSOC_ARRAY_KEY_GT;
    }
}

int assoc_array_key_comp_str(void * k1, void * k2)
{
    int cmp = strcmp((const char *)k1, (const char *)k2);
    if(cmp == 0)
    {
        return ASSOC_ARRAY_KEY_EQ;
    }
    else if(cmp < 0)
    {
        return ASSOC_ARRAY_KEY_LT;
    }
    else
    {
        return ASSOC_ARRAY_KEY_GT;
    }
}

/*
 * For Testing Purposes
 * */

int __assoc_array_validate(struct AssocArrayNode * node)
{
    if(node == NULL)
    {
        return 1;
    }
    if(node->left == NULL && node->right == NULL)
    {
        return (__level(node) == 1);
    }
    if(node->left && (__level(node->left) >= __level(node)))
    {
        return 0;
    }
    if(node->right && (__level(node->right) > __level(node)))
    { 
       return 0;
    }
    if(node->right && node->right->right && (__level(node->right->right) >= __level(node)))
    {
        return 0;
    }
    if(__level(node) > 1 && ((node->left == NULL) || (node->right == NULL)))
    {
        return 0;
    }
    return __assoc_array_validate(node->left) && __assoc_array_validate(node->right);
}

int assoc_array_validate(struct assoc_array * array)
{
    return __assoc_array_validate(array->root);
}

int assoc_array_empty(struct assoc_array * array)
{
    return (array->root == NULL);
}

void * assoc_array_get_self(void * data)
{
    return data;
}

void assoc_array_delete_self(void * data)
{
    free(data);
}
