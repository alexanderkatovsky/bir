#include "fifo.h"
#include "stdio.h"

int main()
{
    struct fifo * list = fifo_create_thread_safe_fifo();
    int i = 0;

    /*pop empty queue*/
    fifo_pop(list);

    printf("length: %d\n",fifo_length(list));
    
    for(;i<10;i++)
    {
        fifo_push(list,(void*)i);
    }

    printf("length: %d\n",fifo_length(list));

    for(i=0;i<5;i++)
    {
        printf("%d\n",(int)fifo_pop(list));
    }

    printf("length: %d\n",fifo_length(list));

    for(i=0;i<2;i++)
    {
        fifo_push(list,(void*)(i+10));
    }

    printf("length: %d\n",fifo_length(list));
        
    for(i=0;i<7;i++)
    {
        printf("%d\n",(int)fifo_pop(list));
    }
    
    printf("length: %d\n",fifo_length(list));

    for(i=0;i<2;i++)
    {
        fifo_push(list,(void*)(i+10));
    }

    printf("length: %d\n",fifo_length(list));
    
    for(i=0;i<2;i++)
    {
        printf("%d\n",(int)fifo_pop(list));
    }

    printf("length: %d\n",fifo_length(list));
    fifo_delete(list,NULL);
}
