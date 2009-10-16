#include "grizly.h"

/*
 * http://www.cs.utk.edu/~cs594np/unp/checksum.html
 */
unsigned short cksum(const unsigned short *data, int len)
{
    long sum = 0;  /* assume 32 bit long, 16 bit short */

    while(len > 1)
    {
        sum += *data++;
        
        if(sum & 0x80000000)
        {
            /* if high order bit set, fold */
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        
        len -= 2;
    }

    if(len)
    {
        /* take care of left over byte */
        sum += *data++;
    }
          
    while(sum>>16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

unsigned short ipheader_checksum(const struct ip * ip_hdr)
{
    struct ip iph = *ip_hdr;
    iph.ip_sum = 0;
    return cksum((unsigned short *)&iph,sizeof(struct ip));
}

unsigned short icmpheader_checksum(const uint8_t * icmp_hdr, unsigned int len)
{
    unsigned short ret = 0;
    uint8_t * data = (uint8_t*) malloc(len);
    memcpy(data,icmp_hdr,len);
    ((struct icmphdr *)data)->checksum = 0;
    ret = cksum((const unsigned short*)data,len);
    free(data);
    return ret;
}
