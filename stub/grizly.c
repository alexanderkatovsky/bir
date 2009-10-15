#include <stdio.h>
#include <stdint.h>

void grizly_dump_packet(uint8_t * packet, unsigned int len)
{
    int i=0;

    while(i < len)
    {
        if(i % 16 == 0)
        {
            printf("\n");
        }

        if(i < len - 1)
        {
            printf("%02x%02x ",packet[i],packet[i+1]);
            i+=2;
        }
        else
        {
            printf("%02x ",packet[i]);
            i++;
        }
    }

    printf("\n");
}
