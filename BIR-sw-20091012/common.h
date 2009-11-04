#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define NEW_STRUCT(varname,structname)                              \
    struct structname * varname =                                   \
        (struct structname * )malloc(sizeof(struct structname));

struct ip_address
{
    uint32_t subnet;
    uint32_t mask;
};

int ip_address_cmp(void * k1, void * k2);


#endif


