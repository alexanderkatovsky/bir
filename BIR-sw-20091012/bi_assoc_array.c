#include "bi_assoc_array.h"
#include <stdlib.h>

void bi_assoc_array_insert(struct bi_assoc_array * array, void * data)
{
    void * a1 = assoc_array_read(array->array_1, array->array_1->get(data));
    void * a2 = assoc_array_read(array->array_2, array->array_2->get(data));

    /*This ensures that the function is bijective*/
    if((a1 == NULL) && (a2 == NULL))
    {
        assoc_array_insert(array->array_1,data);
        assoc_array_insert(array->array_2,data);
    }
}

void * bi_assoc_array_delete_1(struct bi_assoc_array * array, void * key1)
{
    void * ret = NULL;
    
    ret = assoc_array_delete(array->array_1,key1);
    if(ret != NULL)
    {
        assoc_array_delete(array->array_2,array->array_2->get(ret));
    }

    return ret;
}

void * bi_assoc_array_delete_2(struct bi_assoc_array * array, void * key2)
{
    void * ret = NULL;
    
    ret = assoc_array_delete(array->array_2,key2);
    if(ret != NULL)
    {
        assoc_array_delete(array->array_1,array->array_1->get(ret));
    }

    return ret;
}

void bi_assoc_array_delete_array(struct bi_assoc_array * array, void (* delete)(void *))
{
    assoc_array_delete_array(array->array_2,0);
    assoc_array_delete_array(array->array_1,delete);
    free(array);
}

void * bi_assoc_array_read_1(struct bi_assoc_array * array, void * key)
{
    return assoc_array_read(array->array_1,key);
}

void * bi_assoc_array_read_2(struct bi_assoc_array * array, void * key)
{
    return assoc_array_read(array->array_2,key);
}

void bi_assoc_array_walk_array(struct bi_assoc_array * array, int (* fn)(void *, void *), void * user_data)
{
    assoc_array_walk_array(array->array_1,fn,user_data);
}

struct bi_assoc_array * bi_assoc_array_create(assoc_array_key_getter get1, assoc_array_key_comp cmp1,
                                              assoc_array_key_getter get2, assoc_array_key_comp cmp2)
{
    struct bi_assoc_array * ret = (struct bi_assoc_array *)malloc(sizeof(struct bi_assoc_array));
    ret->array_1 = assoc_array_create(get1,cmp1);
    ret->array_2 = assoc_array_create(get2,cmp2);
    return ret;
}

int bi_assoc_array_length(struct bi_assoc_array * array)
{
    return assoc_array_length(array->array_1);
}
