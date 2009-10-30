#ifndef BI_ASSOC_ARRAY_H
#define BI_ASSOC_ARRAY_H

#include "assoc_array.h"

struct bi_assoc_array
{
    struct assoc_array * array_1;
    struct assoc_array * array_2;
};


void bi_assoc_array_insert(struct bi_assoc_array * array, void * data);
void * bi_assoc_array_delete_1(struct bi_assoc_array * array, void * key);
void * bi_assoc_array_delete_2(struct bi_assoc_array * array, void * key);
void bi_assoc_array_delete_array(struct bi_assoc_array * array, void (* delete)(void *));
void * bi_assoc_array_read_1(struct bi_assoc_array * array, void * key);
void * bi_assoc_array_read_2(struct bi_assoc_array * array, void * key);
void bi_assoc_array_walk_array(struct bi_assoc_array * array, int (* fn)(void *, void *), void * user_data);
struct bi_assoc_array * bi_assoc_array_create(assoc_array_key_getter,assoc_array_key_comp,
                                              assoc_array_key_getter,assoc_array_key_comp);

#endif
