#ifndef _MAL_TYPES_H
#define _MAL_TYPES_H

#include "persistent/list/src/list.h"
#include "persistent/hashmap/src/hashmap.h"
#include "persistent/vector/src/vector.h"

#define MALTYPE_SYMBOL 1
#define MALTYPE_KEYWORD 2
#define MALTYPE_INTEGER 3
#define MALTYPE_FLOAT 4
#define MALTYPE_STRING 5
#define MALTYPE_TRUE 6
#define MALTYPE_FALSE 7
#define MALTYPE_NIL 8
#define MALTYPE_LIST 9
#define MALTYPE_VECTOR 10
#define MALTYPE_HASHMAP 11
#define MALTYPE_FUNCTION 12
#define MALTYPE_CLOSURE 13
#define MALTYPE_ERROR 14
#define MALTYPE_ATOM 15

typedef struct MalType_s MalType;
typedef struct MalClosure_s MalClosure;
typedef struct Env_s Env;

struct MalType_s {
  int type;
  int is_macro;
  MalType *metadata;

  union MalValue {
    long mal_integer;
    double mal_float;
    char *mal_symbol;
    char *mal_string;
    char *mal_keyword;
    List *mal_list;
    Vector *mal_vector;
    Hashmap *mal_hashmap;
    MalType *(*mal_function)(List *);
    MalClosure *mal_closure;
    MalType *mal_atom;
    MalType *mal_error;

  } value;
};

struct MalClosure_s {
  Env *env;
  MalType *parameters;
  MalType *more_symbol;
  MalType *definition;
};

hash_t hash_str(void *obj);
int cmp_str(void *val1, void *val2);
int cmp_maltypes(void *val1, void *val2);
List *vector_to_list(Vector *vec);

MalType *make_symbol(char *value);
MalType *make_integer(long value);
MalType *make_float(double value);
MalType *make_keyword(char *value);
MalType *make_string(char *value);
MalType *make_list(List *value);
MalType *make_vector(Vector *value);
MalType *make_hashmap(Hashmap *value);
MalType *make_true(void);
MalType *make_false(void);
MalType *make_nil(void);
MalType *make_atom(MalType *value);
MalType *make_error(char *msg);
MalType *make_error_fmt(char *fmt, ...);
MalType *wrap_error(MalType *value);
MalType *make_function(MalType *(*fn)(List *args));
MalType *make_closure(Env *env, MalType *parameters, MalType *definition, \
                      MalType *more_symbol);
MalType *copy_type(MalType *value);

int is_sequential(MalType *val);
int is_self_evaluating(MalType *val);
int is_list(MalType *val);
int is_vector(MalType *val);
int is_hashmap(MalType *val);
int is_nil(MalType *val);
int is_string(MalType *val);
int is_integer(MalType *val);
int is_float(MalType *val);
int is_number(MalType *val);
int is_true(MalType *val);
int is_false(MalType *val);
int is_symbol(MalType *val);
int is_keyword(MalType *val);
int is_atom(MalType *val);
int is_error(MalType *val);
int is_callable(MalType *val);
int is_function(MalType *val);
int is_closure(MalType *val);
int is_macro(MalType *val);
#endif
