#ifndef ASSOC_ARRAY_H
#define ASSOC_ARRAY_H

/*
 *Provides associative array
 * */

struct AssocArrayNode
{
    int level;
    int key;
    void * data;

    struct AssocArrayNode * right;
    struct AssocArrayNode * left;
};

struct assoc_array
{
    struct AssocArrayNode * root;
};

void assoc_array_insert(struct assoc_array * array, int key, void * data);

/*
 * Delete node at key; return the pointer to the data that was stored in this node
 * */
void * assoc_array_delete(struct assoc_array * array, int key);
void assoc_array_delete_array(struct assoc_array * array, void (* delete)(void *));
void * assoc_array_read(struct assoc_array * array, int key);
void assoc_array_walk_array(struct assoc_array * array, int (* fn)(int, void *, void *), void * user_data);
struct assoc_array * assoc_array_create();
int assoc_array_validate(struct assoc_array * array);

#endif
