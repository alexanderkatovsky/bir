#ifndef ASSOC_ARRAY_H
#define ASSOC_ARRAY_H

/*
 *Provides associative array
 * */

typedef void * ( * assoc_array_key_getter)(void*);
typedef int (* assoc_array_key_comp)(void*,void*);

#define ASSOC_ARRAY_KEY_EQ 0
#define ASSOC_ARRAY_KEY_LT 1
#define ASSOC_ARRAY_KEY_GT (-1)

int assoc_array_key_comp_int(void * k1, void * k2);
int assoc_array_key_comp_str(void * k1, void * k2);

struct assoc_array;

struct AssocArrayNode
{
    int level;
    struct assoc_array * array;
    void * data;

    struct AssocArrayNode * right;
    struct AssocArrayNode * left;
};

struct assoc_array
{
    struct AssocArrayNode * root;
    assoc_array_key_getter get;
    assoc_array_key_comp   cmp;
};

void assoc_array_insert(struct assoc_array * array, void * data);

/*
 * Delete node at key; return the pointer to the data that was stored in this node
 * */
void * assoc_array_delete(struct assoc_array * array, void * key);
void assoc_array_delete_array(struct assoc_array * array, void (* delete)(void *));
void * assoc_array_read(struct assoc_array * array, void * key);
void assoc_array_walk_array(struct assoc_array * array, int (* fn)(void *, void *), void * user_data);
struct assoc_array * assoc_array_create(assoc_array_key_getter,assoc_array_key_comp);
int assoc_array_validate(struct assoc_array * array);

#endif
