#ifndef _MAL_ENV_H
#define _MAL_ENV_H

#include "libs/hashmap/src/hashmap.h"
#include "types.h"

typedef MalType *(*mal_fn)(List *);
typedef struct Env_s Env;

struct Env_s {
  Env *outer;
  Hashmap *data;
};

Env *env_new(Env *outer);
Env *env_make(Env *outer, List *binds, List *exprs, MalType *more_symbol);
Env *env_set(Env *current, MalType *symbol, MalType *value);
Env *env_set_C_fn(Env *current, char *symbol_name, mal_fn fn);
MalType *env_get(Env *current, MalType *symbol);
#endif
