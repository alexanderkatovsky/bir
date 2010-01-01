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

void * assoc_array_get_self(void * data);
void assoc_array_delete_self(void * data);

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

    int total;
};

void assoc_array_insert(struct assoc_array * array, void * data);

/*
 * Delete node at key; return the pointer to the data that was stored in this node
 * */
void * assoc_array_delete(struct assoc_array * array, void * key);
void assoc_array_delete_array(struct assoc_array * array, void (* _delete)(void *));
void * assoc_array_read(struct assoc_array * array, void * key);
void assoc_array_walk_array(struct assoc_array * array, int (* fn)(void *, void *), void * user_data);
struct assoc_array * assoc_array_create(assoc_array_key_getter,assoc_array_key_comp);
int assoc_array_validate(struct assoc_array * array);
int assoc_array_empty(struct assoc_array * array);
int assoc_array_length(struct assoc_array * array);

/* eq tests for equality of data; returns 1 if equal, 0 if not  */
int assoc_array_eq(struct assoc_array * a1, struct assoc_array * a2, int (* eq)(void *, void *));

#endif
