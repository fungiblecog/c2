#ifndef _MAL_CORE_H
#define _MAL_CORE_H

#include "libs/hashmap/src/hashmap.h"
#include "types.h"

typedef struct {
  Hashmap *mappings;
} ns;

int cmp_maltypes(void *data1, void *data2);
ns *ns_make_core(void);

/* forward references */
MalType *apply(MalType *fn, List *args);
MalType *as_str(List *args, int readably, char *separator);
MalType *print(List *args, int readably, char *separator);
MalType *equal_seqs(MalType *seq1, MalType *seq2);
MalType *equal_hashmaps(MalType *map1, MalType *map2);

/* core ns functions */
MalType *mal_add(List *args);
MalType *mal_sub(List *args);
MalType *mal_mul(List *args);
MalType *mal_div(List *args);

MalType *mal_prn(List *args);
MalType *mal_println(List *args);
MalType *mal_pr_str(List *args);
MalType *mal_str(List *args);
MalType *mal_read_string(List *args);
MalType *mal_slurp(List *args);

MalType *mal_list(List *args);
MalType *mal_list_questionmark(List *args);
MalType *mal_empty_questionmark(List *args);
MalType *mal_count(List *args);
MalType *mal_cons(List *args);
MalType *mal_concat(List *args);
MalType *mal_nth(List *args);
MalType *mal_first(List *args);
MalType *mal_rest(List *args);

MalType *mal_equals(List *args);
MalType *mal_lessthan(List *args);
MalType *mal_lessthanorequalto(List *args);
MalType *mal_greaterthan(List *args);
MalType *mal_greaterthanorequalto(List *args);

MalType *mal_atom(List *args);
MalType *mal_atom_questionmark(List *args);
MalType *mal_deref(List *args);
MalType *mal_reset_bang(List *args);
MalType *mal_swap_bang(List *args);

MalType *mal_throw(List *args);
MalType *mal_apply(List *args);
MalType *mal_map(List *args);

MalType *mal_nil_questionmark(List *args);
MalType *mal_true_questionmark(List *args);
MalType *mal_false_questionmark(List *args);
MalType *mal_symbol_questionmark(List *args);
MalType *mal_keyword_questionmark(List *args);
MalType *mal_symbol(List *args);
MalType *mal_keyword(List *args);

MalType *mal_vec(List *args);
MalType *mal_vector(List *args);
MalType *mal_vector_questionmark(List *args);
MalType *mal_sequential_questionmark(List *args);
MalType *mal_hash_map(List *args);
MalType *mal_map_questionmark(List *args);
MalType *mal_assoc(List *args);
MalType *mal_dissoc(List *args);
MalType *mal_get(List *args);
MalType *mal_contains_questionmark(List *args);
MalType *mal_keys(List *args);
MalType *mal_vals(List *args);
MalType *mal_string_questionmark(List *args);
MalType *mal_number_questionmark(List *args);
MalType *mal_fn_questionmark(List *args);
MalType *mal_macro_questionmark(List *args);

MalType *mal_time_ms(List *args);
MalType *mal_conj(List *args);
MalType *mal_seq(List *args);
MalType *mal_meta(List *args);
MalType *mal_with_meta(List *args);

MalType *mal_compile(List *args);

#ifdef WITH_FFI
MalType *mal_dot(List *args);
#endif

#endif
