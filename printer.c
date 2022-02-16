#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc.h>

#include "printer.h"

#define PRINT_NIL "nil"
#define PRINT_TRUE "true"
#define PRINT_FALSE "false"

#define INTEGER_BUFFER_SIZE 16
#define SYMBOL_BUFFER_SIZE 32
#define FUNCTION_BUFFER_SIZE 256
#define STRING_BUFFER_SIZE 256
#define LIST_BUFFER_SIZE 1024

/* forward references */
char *pr_str_sequential(Iterator *iter, int readably, char *start_delimiter, \
                        char *end_delimiter, char *separator);
char *pr_str_list(List *lst, int readably);
char *pr_str_vector(Vector *vec, int readably);
char *pr_str_hashmap(Hashmap *map, int readably);
char *escape_string(char *str);

char *pr_str(MalType *val, int readably) {

  if (!val) { return ""; }

  switch(val->type) {

  case MALTYPE_SYMBOL:

    return snprintfbuf(SYMBOL_BUFFER_SIZE, "%s", val->value.mal_symbol);
    break;

  case MALTYPE_KEYWORD:

    return snprintfbuf(SYMBOL_BUFFER_SIZE, ":%s", val->value.mal_keyword);
    break;

  case MALTYPE_INTEGER:

    return snprintfbuf(SYMBOL_BUFFER_SIZE, "%ld", val->value.mal_integer);
    break;

  case MALTYPE_FLOAT:

    return snprintfbuf(SYMBOL_BUFFER_SIZE, "%lf", val->value.mal_float);
    break;

  case MALTYPE_STRING:

    if (readably) {
      return snprintfbuf(STRING_BUFFER_SIZE, "%s", escape_string(val->value.mal_string));
    }
    else {
      return snprintfbuf(STRING_BUFFER_SIZE, "%s",val->value.mal_string);
    }
    break;

  case MALTYPE_TRUE:

    return PRINT_TRUE;
    break;

  case MALTYPE_FALSE:

    return PRINT_FALSE;
    break;

  case MALTYPE_NIL:

    return PRINT_NIL;
    break;

  case MALTYPE_LIST:

    return pr_str_list(val->value.mal_list, readably);
    break;

  case MALTYPE_VECTOR:

    return pr_str_vector(val->value.mal_vector, readably);
    break;

  case MALTYPE_HASHMAP:

    return pr_str_hashmap(val->value.mal_hashmap, readably);
    break;

  case MALTYPE_FUNCTION:

    return snprintfbuf(FUNCTION_BUFFER_SIZE, "#<function::native@%p>", \
                       val->value.mal_function);
    break;

  case MALTYPE_CLOSURE:
    {
      MalType *definition = (val->value.mal_closure)->definition;
      MalType *parameters = (val->value.mal_closure)->parameters;
      MalType *more_symbol = (val->value.mal_closure)->more_symbol;

      List *lst = parameters->value.mal_list;
      MalType *args;

      /* to pretty print the more symbol it needs to be
         added to the end of the function argument list */
      if (more_symbol) {
        char *symbol = snprintfbuf(STRING_BUFFER_SIZE, "%s%s", "&",     \
                                   more_symbol->value.mal_symbol);

        args = make_list(list_concatenate(lst, list_make(make_symbol(symbol))));
      }
      else {
        args = make_list(lst);
      }

      if (val->is_macro) {
        return snprintfbuf(FUNCTION_BUFFER_SIZE, "#<function::macro: (fn* %s %s))", \
                           pr_str(args, UNREADABLY),          \
                           pr_str(definition, UNREADABLY));
      }
      else {
        return snprintfbuf(FUNCTION_BUFFER_SIZE, "#<function::closure: (fn* %s %s))", \
                           pr_str(args, UNREADABLY),          \
                           pr_str(definition, UNREADABLY));
      }
    }
  break;

  case MALTYPE_ATOM:

    return snprintfbuf(STRING_BUFFER_SIZE, "(atom %s)", \
                       pr_str(val->value.mal_atom, readably));
    break;

  case MALTYPE_ERROR:

    return snprintfbuf(STRING_BUFFER_SIZE, "Uncaught error: %s", \
                       pr_str(val->value.mal_error, UNREADABLY));
    break;

  default:
    /* can't happen unless a new MalType is added */
    return "Printer error: unknown type\n";
    break;
  }
}

char *pr_str_sequential(Iterator *iter, int readably, char *start_delimiter, \
                        char *end_delimiter, char *separator) {

  char *list_buffer = GC_MALLOC(sizeof(*list_buffer) * LIST_BUFFER_SIZE);
  long buffer_length = LIST_BUFFER_SIZE;

  /* add the start delimiter */
  list_buffer = strcpy(list_buffer, start_delimiter);

  long len = strlen(start_delimiter);
  long count = len;

  while (iter) {

    /* concatenate next element */
    MalType *data = iter->value;
    char *str = pr_str(data, readably);

    len = strlen(str);
    count += len;

    if (count >= buffer_length) {
      buffer_length += (count + 1);
      list_buffer = GC_REALLOC(list_buffer, buffer_length);
    }

    strncat(list_buffer, str, len);
    iter = iterator_next(iter);

    if (iter) {
      len = strlen(separator);
      count += len;

      if (count >= buffer_length) {
        buffer_length += (count + 1);
        list_buffer = GC_REALLOC(list_buffer, buffer_length);
      }
      /* add the separator */
      strncat(list_buffer, separator, len);
    }
  }

  if (count >= buffer_length) {
    len = strlen(end_delimiter);
    count += len;

    buffer_length += (count + 1);
    list_buffer = GC_REALLOC(list_buffer, buffer_length);
  }

  /* add the end delimiter */
  strncat(list_buffer, end_delimiter, len);

  return list_buffer;
}

char *pr_str_list(List *lst, int readably) {

  return pr_str_sequential(list_iterator_make(lst), readably, "(", ")", " ");
}

char *pr_str_vector(Vector *vec, int readably) {

  return pr_str_sequential(vector_iterator_make(vec), readably, "[", "]", " ");
}

char *pr_str_hashmap(Hashmap *map, int readably) {

  return pr_str_sequential(hashmap_iterator_make(map), readably, "{", "}", " ");
}

char *escape_string(char *str) {

  /* allocate a reasonable initial buffer size */
  long buffer_length = 2*(strlen(str) + 1);
  char *buffer = GC_MALLOC(sizeof(*buffer) * buffer_length);

  strcpy(buffer,"\"");

  char *curr = str;
  while(*curr != '\0') {

    switch (*curr) {

    case '"':
      strcat(buffer, "\\\"");
      break;

    case '\\':
      strcat(buffer, "\\\\");
      break;

    case 0x0A:
      strcat(buffer, "\\n");
      break;

    default:
      strncat(buffer, curr, 1);
    }
    curr++;

    /* check for overflow and increase buffer size */
    if ((curr - str) >= buffer_length) {
      buffer_length *= 2;
      buffer = GC_REALLOC(buffer, sizeof(*buffer) * buffer_length);
    }
  }

  strcat(buffer, "\"");

  /* trim the buffer to the size of the actual escaped string */
  buffer_length = strlen(buffer);
  buffer = GC_REALLOC(buffer, sizeof(*buffer) * buffer_length + 1);

  return buffer;
}

char *snprintfbuf(long initial_size, char *fmt, ...) {
  /* this is just a wrapper for the *printf family that ensures the
     string is long enough to hold the contents */

  /* needed for variadic function */
  va_list argptr, argptr_copy;

  /* last fixed argument is fmt */
  va_start(argptr, fmt);
  /* make a copy of the va_list to reuse later if need to allocate more space */
  va_copy(argptr_copy, argptr);

  /* allocate an initial buffer plus a char for the /0 */
  char *buffer = GC_MALLOC(sizeof(*buffer) * initial_size + 1);

  /* the return value from vsnprintf is the number of chars that actually need to be allocated
   which could be less than were provided in the buffer */
  long n = vsnprintf(buffer, initial_size, fmt, argptr) + 1;
  va_end(argptr);

  /* if there wasn't enough space reallocate the buffer and try again */
  if (n > initial_size) {

    buffer = GC_REALLOC(buffer, sizeof(*buffer) * n);
    vsnprintf(buffer, n, fmt, argptr_copy);
    va_end(argptr_copy);
  }
  return buffer;
}
