#include <stdio.h>
#include <string.h>
#include <libtcc.h>
#include <gc.h>
#include "env.h"
#include "core.h"
#include "compiler.h"

#define INITIAL_FUNCTION_SIZE 8192
#define GENSYM_SIZE 16
#define STRINGIFY(...) #__VA_ARGS__

/* TODO: put these in a shared header file */
#define SYMBOL_DEFBANG "def!"
#define SYMBOL_LETSTAR "let*"
#define SYMBOL_DO "do"
#define SYMBOL_IF "if"
#define SYMBOL_FNSTAR "fn*"
#define SYMBOL_QUOTE "quote"
#define SYMBOL_QUASIQUOTE "quasiquote"
#define SYMBOL_QUASIQUOTEEXPAND "quasiquoteexpand"
#define SYMBOL_UNQUOTE "unquote"
#define SYMBOL_SPLICE_UNQUOTE "splice-unquote"
#define SYMBOL_DEFMACROBANG "defmacro!"
#define SYMBOL_MACROEXPAND "macroexpand"
#define SYMBOL_TRYSTAR "try*"
#define SYMBOL_CATCHSTAR "catch*"
#define SYMBOL_VEC "vec"
#define SYMBOL_CONCAT "concat"
#define SYMBOL_CONS "cons"


MalType *compile_closure(MalClosure *closure);
MalType *compile_expression(MalType *expr, Env *env, int ret);

MalType *compile_defbang(MalType *expr, Env *env);
MalType *compile_letstar(MalType *expr, Env *env);
MalType *compile_if(MalType *expr, Env *env);
MalType *compile_do(MalType *expr, Env *env);
MalType *compile_application(MalType *expr, Env *env, int ret);

MalType *compile_nil(MalType *expr, Env *env, int ret);
MalType *compile_true(MalType *expr, Env *env, int ret);
MalType *compile_false(MalType *expr, Env *env, int ret);
MalType *compile_integer(MalType *expr, Env *env, int ret);
MalType *compile_string(MalType *expr, Env *env, int ret);
MalType *compile_symbol(MalType *expr, Env *env, int ret);

/* the global environment */
extern Env *global_env;

Env *get_global_env()
{
  return global_env;
}

/* return a unique identifier */
char *gensym(char *prefix)
{

  static int i = 0;
  char *result = GC_MALLOC(sizeof(*result) * GENSYM_SIZE);

  if (prefix) {
    snprintf(result, GENSYM_SIZE, "%s_%i", prefix, i);
  } else {
    snprintf(result, GENSYM_SIZE, "G_%i", i);
  }
  return result;
}

/* a hashmap to store environments needed for compilation */
Hashmap *compilation_envs;

/* environments are the same if they point to the same one */
int cmp_envs(void *val1, void *val2)
{
  return (val1 == val2);
}

/* make a hashmap to store environments */
Hashmap *make_compilation_envs()
{
  return hashmap_make(hash_str, cmp_str, cmp_envs);
}

/* save a compilation environment under a gensym'd name */
char *save_env(Env *env)
{
  char *name = gensym(NULL);
  compilation_envs = hashmap_assoc(compilation_envs, name, env);

  return name;
}

/* get a compilation environment by name */
Env *get_env(char *name)
{
  return hashmap_get(compilation_envs, name);
}

/* compiles a closure fn to machine code via C using libtcc */
MalType *compile(MalType *fn)
{
  if (!compilation_envs) {
    compilation_envs = make_compilation_envs();
  }

  /* compiler needs to create a function to
     implement the closure as a string of C code*/
  MalType *program_string = compile_closure(fn->value.mal_closure);
  if (is_error(program_string)) { return program_string;}

  char *program = program_string->value.mal_string;

  /* create a tcc compilation state */
  TCCState *s = tcc_new();

  if (!s) {
    return make_error("could not create compilation state");
  }

  /* MUST BE CALLED before any compilation */
  tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

  if (tcc_compile_string(s, program) == -1) {
    return make_error("could not compile function");
  }

  /* add symbols that the compiled program can use */
  tcc_add_symbol(s, "make_nil", make_nil);
  tcc_add_symbol(s, "make_true", make_true);
  tcc_add_symbol(s, "make_false", make_false);
  tcc_add_symbol(s, "is_nil", is_nil);
  tcc_add_symbol(s, "is_true", is_true);
  tcc_add_symbol(s, "is_false", is_false);
  tcc_add_symbol(s, "is_function", is_function);
  tcc_add_symbol(s, "is_error", is_error);
  //  tcc_add_symbol(s, "is_", is_);

  tcc_add_symbol(s, "env_make", env_make);
  tcc_add_symbol(s, "env_get", env_get);
  tcc_add_symbol(s, "env_set", env_set);

  tcc_add_symbol(s, "make_integer", make_integer);
  tcc_add_symbol(s, "make_string", make_string);
  tcc_add_symbol(s, "make_symbol", make_symbol);
  tcc_add_symbol(s, "make_list", make_list);
  tcc_add_symbol(s, "make_error", make_error);
  tcc_add_symbol(s, "make_error_fmt", make_error_fmt);
  //  tcc_add_symbol(s, "make_", make_);

  tcc_add_symbol(s, "list_make", list_make);
  tcc_add_symbol(s, "list_cons", list_cons);
  tcc_add_symbol(s, "list_reverse", list_reverse);
  tcc_add_symbol(s, "list_count", list_count);

  tcc_add_symbol(s, "get_global_env", get_global_env);

  //  tcc_add_symbol(s, "mal_", mal_);

  /* get the size of the generated code */
  int size = tcc_relocate(s, NULL);
  if (size == -1) {
    return make_error("could not relocate compiled function");
  }

  /* allocate memory and copy the code into it */
  void *mem = GC_MALLOC(size);
  if (tcc_relocate(s, mem) < 0) {
    return make_error("could not relocate compiled function");
  }

  /* function signature */
  MalType* (*closure_func)(List *args);

  /* get the compiled function */
  closure_func = tcc_get_symbol(s, "closure_func");
  if (!closure_func) {
    return make_error("could not get function symbol");
  }

  /* delete the state */
  tcc_delete(s);

  /* return the function */
  return make_function(closure_func);
}

MalType *compile_closure(MalClosure *closure)
{
  /* extract closure components */
  MalType *definition = closure->definition;
  Env *env = closure->env;
  MalType *parameters = closure->parameters;
  //MalType *more = closure->more_symbol;

  MalType *prog = compile_expression(definition, env, 0);

  if (is_error(prog)) { return prog; }

  List *lst = parameters->value.mal_list;

  /* create definitions for the arguments */
  char *param_i = GC_MALLOC(sizeof(*param_i) * INITIAL_FUNCTION_SIZE);
  char *params = GC_MALLOC(sizeof(*params) * INITIAL_FUNCTION_SIZE);

  while (lst) {
    MalType *p = lst->data;
    snprintf(param_i, INITIAL_FUNCTION_SIZE,
             "params = list_cons(params, make_symbol(\"%s\"));\n",
             p->value.mal_symbol);
    strcat(params, param_i);
    lst = lst->next;
  }

  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "#include <stddef.h>\n"
           "#include \"types.h\"\n"
           "#include \"env.h\"\n"
           "#include \"core.h\"\n"
           "Env *get_global_env();\n"
           "MalType *closure_func(List *args) {\n"
           "MalType *result;\n"
           "List *params = NULL;\n"
           "%s"
           "params = list_reverse(params);\n"
           "Env *global_env = get_global_env();\n"
           "int param_count = list_count(params);"
           "if (list_count(args) != param_count) {\n"
           "return make_error_fmt(\"Error: expected \%%i argument\%%s\","
           "param_count, (param_count == 1) ? \"\" : \"s\");}"
           "Env *closure_env = env_make(global_env, params, args, NULL);\n"
           "%s"
           "return result;\n}\n",
           params, prog->value.mal_string);

  return make_string(code);
}

MalType *compile_expression(MalType *expr, Env *env, int ret)
{
  MalType *buffer;

  if (is_list(expr)) {

    MalType *first = (expr->value.mal_list)->data;
    char *symbol = first->value.mal_symbol;

    if (is_symbol(first)) {

      /* handle special symbols first */
      if (strcmp(symbol, SYMBOL_DEFBANG) == 0) {

        buffer = compile_defbang(expr, env);
      }
      else if (strcmp(symbol, SYMBOL_LETSTAR) == 0) {

        buffer = compile_letstar(expr, env);
      }
      else if (strcmp(symbol, SYMBOL_IF) == 0) {

        buffer = compile_if(expr, env);
      }
      else if (strcmp(symbol, SYMBOL_DO) == 0) {

        buffer = compile_do(expr, env);
      }
      else {
        buffer = compile_application(expr, env, 0);
      }
    }
  }
  else if (is_nil(expr)) {
      buffer = compile_nil(expr, env, 0);
  }
  else if (is_true(expr)) {
      buffer = compile_true(expr, env, 0);
  }
  else if (is_false(expr)) {
      buffer = compile_false(expr, env, 0);
  }
  else if (is_integer(expr)) {

    buffer = compile_integer(expr, env, 0);
  }
  else if (is_string(expr)) {

    buffer = compile_string(expr, env, 0);
    }
  else if (is_symbol(expr)) {

    buffer = compile_symbol(expr, env, 0);
  }
  else {
    return make_error("Compiler error: Unknown atom");
  }
  return buffer;
}

MalType *compile_application(MalType *expr, Env *env, int ret)
{
  List *lst = expr->value.mal_list;
  MalType *fn = lst->data;

  /* advance to the first argument */
  if (lst->next) { lst = lst->next; }

  /* compile the arguments */
  List *comp_lst = NULL;
  while (lst) {
    MalType *arg_n = compile_expression(lst->data, env, 0);
    comp_lst = list_cons(comp_lst, arg_n);
    lst = lst->next;
  }
  comp_lst = list_reverse(comp_lst);

  /* create definitions for the arguments */
  char *def_i = GC_MALLOC(sizeof(*def_i) * INITIAL_FUNCTION_SIZE);
  char *defs = GC_MALLOC(sizeof(*defs) * INITIAL_FUNCTION_SIZE);

  while (comp_lst) {
    MalType *arg = comp_lst->data;
    snprintf(def_i, INITIAL_FUNCTION_SIZE,
             "arg_vals = list_cons(arg_vals, %s);\n",
             arg->value.mal_string);
    strcat(defs, def_i);
    comp_lst = comp_lst->next;
  }

  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "/* compile application - start */\n"
           "result = make_nil();\n"
           "MalType *fn = env_get(closure_env, make_symbol(\"%s\"));\n"
           "if (is_error(fn)) { return fn; }\n"
           "List *arg_vals = NULL;\n"
           "/* arg definitions - start */\n"
           "%s"
           "arg_vals = list_reverse(arg_vals);\n"
           "/* arg definitions - end */\n"
           "result = (*fn->value.mal_function)(arg_vals);\n",
           fn->value.mal_symbol, defs);

  return make_string(code);
}

MalType *compile_defbang(MalType *expr, Env *env)
{
  return make_error("Compiler: 'def!' not implemented");
}

MalType *compile_letstar(MalType *expr, Env *env)
{
  return make_error("Compiler: 'let*' not implemented");
}

MalType *compile_if(MalType *expr, Env *env)
{
  List *lst = expr->value.mal_list;

  /* compile the condition expression */
  MalType *condition = compile_expression(lst->next->data, env, 0);
  if (is_error(condition)) { return condition; }

  /* compile the 'true' branch */
  MalType *true_branch = compile_expression(lst->next->next->data, env, 0);
  if (is_error(true_branch)) { return true_branch; }

  /* compile the (optional) 'false' branch */
  MalType *false_branch = NULL;
  if (lst->next->next->next) {
    false_branch = compile_expression(lst->next->next->next->data, env, 0);
    if (is_error(false_branch)) { return false_branch; }
  }

  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "/* if condition - start */\n"
           "%s\n"
           "MalType *cond = result;\n"
           "/* if condition - end */\n"
           "    if (!is_false(cond) && !is_nil(cond)) {\n"
           "/* if true branch - start */\n"
           "       result = %s;"
           "/* if true branch - end */\n"
           "/* if false branch - start */\n"
           "    } else {\n"
           "       result = %s;"
           "/* if false branch - end */\n"
           "    }\n",
           condition->value.mal_string,
           true_branch->value.mal_string,
           !false_branch ? "make_nil()" : false_branch->value.mal_string);

  return make_string(code);
}

MalType *compile_do(MalType *expr, Env *env)
{
  /* TODO: create a block and compile expressions
     sequentially returning the last value */
  return make_error("Compiler: 'do' not implemented");
}

MalType *compile_symbol(MalType *expr, Env *env, int ret)
{
  /* get the value from the environment */
  char *sym = expr->value.mal_symbol;
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);

  if (ret) {
    /* assigns the symbol value to variable 'result' */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "result = env_get(closure_env, make_symbol(\"%s\"));\n", sym);
  } else {
    /* returns the symbol value */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "env_get(closure_env, make_symbol(\"%s\"))", sym);
  }
  return make_string(code);
}

MalType *compile_nil(MalType *expr, Env *env, int ret)
{
  if (ret) {
    /* assigns 'nil' to the variable 'result' */
    return make_string("result = make_nil();");

  } else {
    /* returns 'nil' */
    return make_string("make_nil()");
  }
}

MalType *compile_true(MalType *expr, Env *env, int ret)
{
  if (ret) {
    /* assigns 'trure' to the variable 'result' */
    return make_string("result = make_true();");

  } else {
    /* returns 'true' */
    return make_string("make_true()");
  }
}

MalType *compile_false(MalType *expr, Env *env, int ret)
{
  if (ret) {
    /* assigns 'false' to the variable 'result' */
    return make_string("result = make_false();");

  } else {
    /* returns 'false' */
    return make_string("make_false()");
  }
}

MalType *compile_integer(MalType *expr, Env *env, int ret)
{
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  if (ret) {
    /* assigns the integer to the variable 'result' */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "result = make_integer(%li);\n",
             expr->value.mal_integer);
  } else {
    /* returns the integer */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "make_integer(%li)",
             expr->value.mal_integer);
  }
  return make_string(code);
}

MalType *compile_string(MalType *expr, Env *env, int ret)
{
  /* assigns the string to the variable 'result' */
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  if (ret) {
    /* assigns the string to the variable 'result' */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "result = make_string(%s);\n",
             expr->value.mal_string);

  } else {
    /* returns the string */
    snprintf(code, INITIAL_FUNCTION_SIZE,
             "make_string(%s)",
             expr->value.mal_string);
  }
  return make_string(code);
}
