#ifndef _MAL_CORE_H
#define _MAL_CORE_H

#include "libs/hashmap/src/hashmap.h"
#include "types.h"

typedef struct {
  Hashmap *mappings;
} ns;

int cmp_maltypes(void *data1, void *data2);
ns *ns_make_core(void);
#endif
