#include <stdio.h>
#include <gc.h>

#include "types.h"
#include "env.h"
#include "reader.h"

Env *env_new(Env *outer) {

  Env *env = GC_MALLOC(sizeof(*env));
  env->outer = outer;
  env->data = hashmap_make(hash_str, cmp_str, cmp_maltypes);

  return env;
}

/* Note: caller must make sure enough exprs to match symbols */
Env *env_make(Env *outer, List *symbol_list, List *exprs_list, MalType *more_symbol) {

  Env *env = GC_MALLOC(sizeof(*env));
  env->outer = outer;
  env->data = hashmap_make(hash_str, cmp_str, cmp_maltypes);

  while (symbol_list) {

    env = env_set(env, symbol_list->data, exprs_list->data);

    symbol_list = symbol_list->next;
    exprs_list = exprs_list->next;
  }

  /* set the 'more' symbol if there is one */
  if (more_symbol) {
    env = env_set(env, more_symbol, make_list(exprs_list));
  }
  return env;
}

Env *env_set(Env *current, MalType *symbol, MalType *value) {

  current->data = hashmap_assoc(current->data, symbol->value.mal_symbol, value);
  return current;
}

MalType *env_get(Env *current, MalType *symbol) {

  MalType *val = hashmap_get(current->data, symbol->value.mal_symbol);

  if(val) {
    return val;
  }
  else if (current->outer) {
    return env_get(current->outer, symbol);
  }
  else {
      return make_error_fmt("symbol '%s' not found in environment",
                            symbol->value.mal_symbol);
  }
}

Env *env_set_C_fn(Env *current, char *symbol_name, MalType *(*fn)(List *)) {

  return env_set(current, make_symbol(symbol_name), make_function(fn));
}
