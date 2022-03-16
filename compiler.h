#ifndef _MAL_COMPILER_H
#define _MAL_COMPILER_H

#include <stddef.h>
#include "types.h"
#include "env.h"
#include "core.h"

MalType *EVAL(MalType *ast, Env *env);
Env *get_global_env(void);
List *push_env(List *stack, Env *env);
List *pop_env(List *stack);
Env *peek_env(List *stack);

#endif
