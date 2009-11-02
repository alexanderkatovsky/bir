#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NEW_STRUCT(varname,structname)                              \
    struct structname * varname =                                   \
        (struct structname * )malloc(sizeof(struct structname));


#endif


