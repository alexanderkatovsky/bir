#ifndef DEBUG_H
#define DEBUG_H

typedef int (*print_t)(const char *, ...);

void dump_mac(const uint8_t * mac);
void dump_ip(uint32_t ip);
void dump_raw(const uint8_t * packet, unsigned int len);
void dump_packet(const uint8_t * packet, unsigned int len);

void print_ip(uint32_t ip,print_t print);
void print_mac(const uint8_t * mac,print_t print);

#endif
