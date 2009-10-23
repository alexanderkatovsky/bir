#include "assoc_array.h"
#include <stdlib.h>


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

struct AssocArrayNode * __assoc_array_insert(struct AssocArrayNode * node, int key, void * data)
{
    if(node == NULL)
    {
        node = (struct AssocArrayNode *) malloc(sizeof(struct AssocArrayNode));
        node->data = data;
        node->key = key;
        node->level = 1;
        node->right = NULL;
        node->left = NULL;
    }
    else
    {
        if(key == node->key)
        {
            node->data = data;
        }
        else if(key < node->key)
        {
            node->left = __assoc_array_insert(node->left,key,data);
        }
        else if(key > node->key)
        {
            node->right = __assoc_array_insert(node->right,key,data);
        }
        node = __skew(node);
        node = __split(node);
    }
    return node;
}

void assoc_array_insert(struct assoc_array * array, int key, void * data)
{
    array->root = __assoc_array_insert(array->root,key,data);
}

#define __level(node) ((node) ? ((node)->level):0)

struct AssocArrayNode * __assoc_array_delete(struct AssocArrayNode * node, void ** data, int key)
{
    struct AssocArrayNode * succ;
    
    if(node)
    {
        if(key == node->key)
        {
            *data = node->data;
            if(node->left && node->right)
            {
                succ = node->left;
                while(succ->right)
                {
                    succ = succ->right;
                }
                node->key = succ->key;
                node->left = __assoc_array_delete(node->left,&node->data,node->key);
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
            if(node->key < key)
            {
                node->right = __assoc_array_delete(node->right,data,key);
            }
            else
            {
                node->left = __assoc_array_delete(node->left,data,key);
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

void * assoc_array_delete(struct assoc_array * array, int key)
{
    void * data = NULL;
    array->root = __assoc_array_delete(array->root,&data,key);
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
}

void * __assoc_array_read(struct AssocArrayNode * node, int key)
{
    void * ret = NULL;
    if(node)
    {
        if(node->key == key)
        {
            ret = node->data;
        }
        else if(node->key < key)
        {
            ret = __assoc_array_read(node->right,key);
        }
        else
        {
            ret = __assoc_array_read(node->left,key);
        }
    }
    return ret;
}

void * assoc_array_read(struct assoc_array * array, int key)
{
    struct AssocArrayNode * root = array->root;
    return __assoc_array_read(root,key);
}

void __assoc_array_walk(int * finished, struct AssocArrayNode * node, int (* fn)(int, void *, void *), void * user_data)
{
    if(node)
    {
        *finished = fn(node->key,node->data,user_data);
        if(*finished == 0)
        {
            __assoc_array_walk(finished,node->left,fn,user_data);
        }
        if(*finished == 0)
        {
            __assoc_array_walk(finished,node->right,fn,user_data);
        }
    }
}

void assoc_array_walk_array(struct assoc_array * array, int (* fn)(int, void *, void *), void * user_data)
{
    struct AssocArrayNode * root = array->root;
    int finished = 0;
    __assoc_array_walk(&finished,root,fn,user_data);
}

struct assoc_array * assoc_array_create()
{
    struct assoc_array * array = (struct assoc_array *)malloc(sizeof(struct assoc_array));
    array->root = NULL;
    return array;
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
