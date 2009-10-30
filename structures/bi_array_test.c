#include "bi_assoc_array.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
struct bi_data
{
    int number;
    char name[20]; 
};

void * get1(void * data)
{
    return &((struct bi_data *)data)->number;
}

void * get2(void * data)
{
    return ((struct bi_data *)data)->name;
}

int main()
{
    struct bi_assoc_array * bi =
        bi_assoc_array_create(get1,assoc_array_key_comp_int,get2,assoc_array_key_comp_str);
    struct bi_data * bd1;
    struct bi_data * bd2;
    #define LEN 10
    struct bi_data bid[LEN];
    int i;

    for(i = 0; i < LEN; i++)
    {
        bid[i].number = i;
        sprintf(bid[i].name,"%d",i);
        bi_assoc_array_insert(bi,&bid[i]);
    }

    for(i = 0; i < LEN; i++)
    {
        printf("%s:%d\n",((struct bi_data *)bi_assoc_array_read_1(bi,&bid[i].number))->name,
               ((struct bi_data *)bi_assoc_array_read_2(bi,bid[i].name))->number);
    }

    bi_assoc_array_delete_1(bi,&bid[1].number);
    bi_assoc_array_delete_2(bi,bid[6].name);

    for(i = 0; i < LEN; i++)
    {
        bd1 = bi_assoc_array_read_1(bi,&bid[i].number);
        bd2 = bi_assoc_array_read_1(bi,&bid[i].number);
        assert(bd1 == bd2);
        if(bd1 != NULL)
        {
            printf("%s:%d\n",bd1->name,bd2->number);
        }
    }

    bi_assoc_array_delete_array(bi,0);
    return 0;
}
