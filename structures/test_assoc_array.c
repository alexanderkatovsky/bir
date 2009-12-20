#include "assoc_array.h"
#include <stdio.h>
#include <stdlib.h>

struct data
{
    int key;
    int data;
};


int array_walker(void * dd, void * user_data)
{
    struct data * d = (struct data *)dd;
    printf("(%d,%d)",d->key,d->data);
    return 0;
}


void * getKey(void * d)
{
    return &(((struct data *)d)->key);
}

int eq(void * da1, void * da2)
{
    struct data * d1 = (struct data *)da1;
    struct data * d2 = (struct data *)da2;

    return (d1->data == d2->data && d1->key == d2->key);
}

void testeq()
{
    struct data dd1[10];
    struct data dd2[10];
    int i;

    for(i = 0; i < 10; i++)
    {
        dd1[i].key = i;
        dd1[i].data = i * i;
    }

    for(i = 0; i < 10; i++)
    {
        dd2[i].key = i;
        dd2[i].data = i * i;
    }    
    
    struct assoc_array * array1 = assoc_array_create(getKey,assoc_array_key_comp_int);
    struct assoc_array * array2 = assoc_array_create(getKey,assoc_array_key_comp_int);

    assoc_array_insert(array1,&dd1[0]);
    assoc_array_insert(array1,&dd1[1]);
    assoc_array_insert(array1,&dd1[2]);


    assoc_array_insert(array2,&dd1[1]);
    assoc_array_insert(array2,&dd1[2]);
    assoc_array_delete(array2,&dd1[2]);
    assoc_array_insert(array2,&dd1[5]);
    assoc_array_insert(array2,&dd1[2]);
    assoc_array_insert(array2,&dd1[3]);

    assoc_array_delete(array2,&dd2[3]);

    printf("arrays eq: %d\n", assoc_array_eq(array1, array2, eq));
}


void basictest1()
{
    struct assoc_array * array = assoc_array_create(getKey,assoc_array_key_comp_int);
    int i = 0;

    printf("\nlen (0): %d\n", assoc_array_length(array));

    struct data d1 = {1,1};
    struct data d2 = {2,4};
    struct data d3 = {3,9};
    struct data d4 = {4,16};

    struct data * dd;
    
    assoc_array_insert(array,&d3);
    printf("1. %x\n",assoc_array_read(array,&d3.key));
    printf("\nlen (1): %d\n", assoc_array_length(array));
    assoc_array_insert(array,&d2);
    printf("\nlen (2): %d\n", assoc_array_length(array));
    printf("2. %x\n",assoc_array_read(array,&d3.key));
    assoc_array_insert(array,&d1);
    assoc_array_insert(array,&d1);    
    printf("3. %x\n",assoc_array_read(array,&d3.key));
    printf("\nlen (3): %d\n", assoc_array_length(array));
    
/*
    printf("%x\n",array->root);
    printf("%x\n",array->root->left);
    printf("%x\n",array->root->right);
*/
    assoc_array_delete(array, &d3);
    assoc_array_delete(array, &d3);
    printf("\nlen (2): %d\n", assoc_array_length(array));
    assoc_array_insert(array,&d3);
    printf("3. %x\n",assoc_array_read(array,&d3.key));
    printf("\nlen (3): %d\n", assoc_array_length(array));

    assoc_array_delete_array(array, 0);
    
    /*dd = assoc_array_read(array,&d3.key);
      printf("%d,%d\n\n",dd->key, dd->data);*/

    /* assoc_array_walk_array(array,array_walker,0);printf("\n");    */
}

void testrand(int len)
{
    struct assoc_array * array = assoc_array_create(getKey,assoc_array_key_comp_int);
    struct data * input = (struct data*)malloc(len*sizeof(struct data));
    int i;
    struct data * dd;
    for(i = 0; i < len;i++)
    {
        input[i].key = rand();
        input[i].data = input[i].key % 3412;
        assoc_array_insert(array,&input[i]);
    }
    /*assoc_array_walk_array(array,array_walker,0);printf("\n");    */
    printf("valid? %d\n", assoc_array_validate(array));
    for(i = 0; i < len; i++)
    {
        if(((struct data *)assoc_array_read(array,&input[i].key))->data != input[i].data)
        {
            printf("read error read!\n");
        }
    }
    for(i = 0; i < len / 4; i++)
    {
        assoc_array_delete(array,&input[i].key);
    }
    
    for(i = 0; i < len / 4; i++)
    {
        assoc_array_delete(array,&input[i].key);
    }
    printf("valid? %d\n", assoc_array_validate(array));
    
    for(i = 0; i < len / 4; i++)
    {
        if(assoc_array_read(array,&input[i].key) != NULL)
        {
            printf("delete failure\n");
        }
    }
    for(i = 0; i < len; i++)
    {
        dd = (struct data *)assoc_array_read(array,&input[i].key);
        if(i < len / 4)
        {
            if(dd != NULL)
            {
                printf("read error NULL! %d\n",i);
            }
        }
        else if((dd == NULL) || dd->data != input[i].data)
        {
            printf("read error or index clash at %d with %d\n",i,input[i].key);
        }
    }
    printf("valid? %d\n", assoc_array_validate(array));
    assoc_array_delete_array(array,NULL);
    free(input);
}

int main()
{
/*    testrand(1000057);
      basictest1();*/

    testeq();

    return 0;
}
