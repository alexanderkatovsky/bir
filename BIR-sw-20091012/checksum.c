#include "router.h"
#include <stdlib.h>
#include <string.h>
/*
 * cksum taken from
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
        sum += *data & 0x00ff;
    }
          
    while(sum>>16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

unsigned short checksum_ipheader(const struct ip * ip_hdr)
{
    struct ip iph = *ip_hdr;
    iph.ip_sum = 0;
    return cksum((unsigned short *)&iph,sizeof(struct ip));
}

unsigned short checksum_icmpheader(const uint8_t * icmp_hdr, unsigned int len)
{
    unsigned short ret = 0;
    uint8_t * data = (uint8_t*) malloc(len);
    memcpy(data,icmp_hdr,len);
    ((struct icmphdr *)data)->checksum = 0;
    ret = cksum((const unsigned short*)data,len);
    free(data);
    return ret;
}

unsigned short checksum_ospfheader(const uint8_t * p_ospf_hdr, unsigned int len)
{
    unsigned short ret = 0;
    uint8_t * data = (uint8_t*) malloc(len);
    memcpy(data,p_ospf_hdr,sizeof(struct ospfv2_hdr) - 8);
    memcpy(data + sizeof(struct ospfv2_hdr) - 8,p_ospf_hdr + sizeof(struct ospfv2_hdr),
           len-  sizeof(struct ospfv2_hdr));  
    ((struct ospfv2_hdr *)data)->csum = 0;
    ret = cksum((const unsigned short*)data,len - 8);
    free(data);
    return ret;
}
