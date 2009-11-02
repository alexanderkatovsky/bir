#ifndef ETH_HEADERS_H
#define ETH_HEADERS_H


#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define ICMP_REPLY 0
#define ICMP_REQUEST 8
#define ICMP_TIME_EXCEEDED 11

struct icmphdr
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__ ((packed)) ;

struct tcpheader {
    unsigned short int   tcph_srcport;
    unsigned short int   tcph_destport;
    unsigned int     tcph_seqnum;
    unsigned int     tcph_acknum;
    unsigned char    tcph_reserved:4, tcph_offset:4;
    unsigned char    tcph_flags;
    unsigned short int   tcph_win;
    unsigned short int   tcph_chksum;
    unsigned short int   tcph_urgptr;
} __attribute__ ((packed)) ;

#define IP_P_ICMP 1
#define IP_P_TCP  6
#define IP_P_OSPF 89
/*
 * Structure of an internet header, naked of options.
 */
struct ip
{
    unsigned int ip_hl:4;		/* header length */
    unsigned int ip_v:4;		/* version */
    uint8_t ip_tos;			/* type of service */
    uint16_t ip_len;			/* total length */
    uint16_t ip_id;			/* identification */
    uint16_t ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
    uint8_t ip_ttl;			/* time to live */
    uint8_t ip_p;			/* protocol */
    uint16_t ip_sum;			/* checksum */
    struct in_addr ip_src, ip_dst;	/* source and dest address */
} __attribute__ ((packed)) ;

/* 
 *  Ethernet packet header prototype.  Too many O/S's define this differently.
 *  Easy enough to solve that and define it here.
 */
struct sr_ethernet_hdr
{
#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif
    uint8_t  ether_dhost[ETHER_ADDR_LEN];    /* destination ethernet address */
    uint8_t  ether_shost[ETHER_ADDR_LEN];    /* source ethernet address */
    uint16_t ether_type;                     /* packet type ID */
} __attribute__ ((packed)) ;

#ifndef ARPHDR_ETHER
#define ARPHDR_ETHER    1
#endif

#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP            0x0001  /* ICMP protocol */
#endif

#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP            0x0800  /* IP protocol */
#endif

#ifndef ETHERTYPE_ARP
#define ETHERTYPE_ARP           0x0806  /* Addr. resolution protocol */
#endif

#define ARP_REQUEST 1
#define ARP_REPLY   2

struct sr_arphdr 
{
    unsigned short  ar_hrd;             /* format of hardware address   */
    unsigned short  ar_pro;             /* format of protocol address   */
    unsigned char   ar_hln;             /* length of hardware address   */
    unsigned char   ar_pln;             /* length of protocol address   */
    unsigned short  ar_op;              /* ARP opcode (command)         */
    unsigned char   ar_sha[ETHER_ADDR_LEN];   /* sender hardware address      */
    uint32_t        ar_sip;             /* sender IP address            */
    unsigned char   ar_tha[ETHER_ADDR_LEN];   /* target hardware address      */
    uint32_t        ar_tip;             /* target IP address            */
} __attribute__ ((packed)) ;



#endif
