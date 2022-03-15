#include <stdio.h>
#include <string.h>
#include <libtcc.h>
#include <gc.h>
#include "env.h"
#include "core.h"
#include "compiler.h"

#define INITIAL_FUNCTION_SIZE 8192
#define DEFAULT_GENSYM_PREFIX "G"
#define GENSYM_SIZE 32

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
MalType *compile_expression(MalType *expr, Env *env);

MalType *compile_defbang(MalType *expr, Env *env);
MalType *compile_letstar(MalType *expr, Env *env);
MalType *compile_if(MalType *expr, Env *env);
MalType *compile_do(MalType *expr, Env *env);
MalType *compile_application(MalType *expr, Env *env);

MalType *compile_nil(MalType *expr, Env *env);
MalType *compile_true(MalType *expr, Env *env);
MalType *compile_false(MalType *expr, Env *env);
MalType *compile_integer(MalType *expr, Env *env);
MalType *compile_string(MalType *expr, Env *env);
MalType *compile_symbol(MalType *expr, Env *env);

MalType *EVAL(MalType *ast, Env *env);

/* the global environment */
extern Env *global_env;

/* tcclib cannot access the global_env variable
   so need a function to access it */
Env *get_global_env()
{
  return global_env;
}

/* return a unique identifier */
char *gensym(char *prefix)
{
  static int i = 0;
  if (!prefix) { prefix = DEFAULT_GENSYM_PREFIX; }

  char *result = GC_MALLOC(sizeof(*result) * GENSYM_SIZE);
  snprintf(result, GENSYM_SIZE, "%s_%i", prefix, i++);

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
  char *name = gensym("GENV");
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
  tcc_add_symbol(s, "is_closure", is_closure);
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

  tcc_add_symbol(s, "get_env", get_env);
  tcc_add_symbol(s, "save_env", save_env);

  tcc_add_symbol(s, "EVAL", EVAL);

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
  MalType *prog = compile_expression(closure->definition, closure->env);
  if (is_error(prog)) { return prog; }

  /* create a new environment holding the parameter list */
  Env *env = env_make(closure->env, NULL, NULL, NULL);
  env = env_set(env, make_symbol("params"), closure->parameters);

  /* stash the environment where the compiled function can get it */
  char *name = save_env(env);

  /* generate the C code for the closure */
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "#include <stddef.h>\n"
           "#include \"types.h\"\n"
           "#include \"env.h\"\n"
           "#include \"core.h\"\n"
           "MalType *EVAL(MalType *ast, Env *env);\n"
           "Env *get_env(char* name);\n"
           "\n"
           "MalType *closure_func(List *args) {\n"
           "MalType *result;\n"
           "Env *env = get_env(\"%1$s\");\n"
           "MalType *params = env_get(env, make_symbol(\"params\"));\n"
           "int param_count = list_count(params->value.mal_list);\n"
           "if (list_count(args) != param_count) {\n"
           "return make_error_fmt(\"Error: expected \%%i argument\%%s\","
           "param_count, (param_count == 1) ? \"\" : \"s\");}\n"
           "Env *closure_env = env_make(env, params->value.mal_list, args, NULL);\n"
           "%2$s"
           "return result;\n}\n",
           name, prog->value.mal_string);

  return make_string(code);
}

MalType *compile_expression(MalType *expr, Env *env)
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
        buffer = compile_application(expr, env);
      }
    }
  }
  else if (is_nil(expr)) {
      buffer = compile_nil(expr, env);
  }
  else if (is_true(expr)) {
      buffer = compile_true(expr, env);
  }
  else if (is_false(expr)) {
      buffer = compile_false(expr, env);
  }
  else if (is_integer(expr)) {

    buffer = compile_integer(expr, env);
  }
  else if (is_string(expr)) {

    buffer = compile_string(expr, env);
    }
  else if (is_symbol(expr)) {

    buffer = compile_symbol(expr, env);
  }
  else {
    return make_error("Compiler error: Unknown atom");
  }
  return buffer;
}

List *compile_list(List *lst, Env *env) {

  List *compiled_lst = NULL;
  while (lst) {
    MalType *expr = compile_expression(lst->data, env);
    compiled_lst = list_cons(compiled_lst, expr);
    lst = lst->next;
  }
  return list_reverse(compiled_lst);
}

MalType *compile_defbang(MalType *expr, Env *env)
{
  return make_error("Compiler: 'def!' not implemented");
}

MalType *compile_application(MalType *expr, Env *env)
{
  List *lst = expr->value.mal_list;
  MalType *fn = lst->data;

  /* advance to the first argument */
  if (lst->next) { lst = lst->next; }

  /* compile the list of arguments */
  List *comp_lst = compile_list(lst, env);

  /* create definitions for the arguments */
  char *def_i = GC_MALLOC(sizeof(*def_i) * INITIAL_FUNCTION_SIZE);
  char *defs = GC_MALLOC(sizeof(*defs) * INITIAL_FUNCTION_SIZE);

  char *list_name = gensym("arg_vals");
  while (comp_lst) {
    MalType *arg = comp_lst->data;
        snprintf(def_i, INITIAL_FUNCTION_SIZE,
             "%1$s"
             "%2$s = list_cons(%2$s, result);\n",
             arg->value.mal_string, list_name);
    strcat(defs, def_i);
    comp_lst = comp_lst->next;
  }

  char *fn_name = gensym("fn");
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "/* application - start */\n"
           "MalType *%1$s = env_get(closure_env, make_symbol(\"%2$s\"));\n"
           "if (is_error(%1$s)) { return %1$s; }\n"
           "List *%3$s = NULL;\n"
           "%4$s"
           "%3$s = list_reverse(%3$s);\n"
           "if (is_function(%1$s)) {\n"
           "result = (%1$s->value.mal_function)(%3$s);\n"
           "} else if (is_closure(%1$s)) {\n"
           "MalClosure *c = %1$s->value.mal_closure;\n"
           "List *p = (c->parameters)->value.mal_list;\n"
           "Env *e = env_make(c->env, p, %3$s, c->more_symbol);\n"
           "result = EVAL(c->definition, e);\n"
           "} else {\n"
           "return make_error(\"not a callable procedure\");}\n"
           "/* application - end */\n",
           fn_name, fn->value.mal_symbol, list_name, defs);

  return make_string(code);
}

MalType *compile_do(MalType *expr, Env *env)
{
  List *lst = expr->value.mal_list;

  /* advance to the first expression */
  if (lst->next) { lst = lst->next; }

  /* compile the expressions */
  List *exp_lst = compile_list(lst, env);

  /* sequence the compiled expressions */
  char *exp_i = GC_MALLOC(sizeof(*exp_i) * INITIAL_FUNCTION_SIZE);
  char *exps = GC_MALLOC(sizeof(*exps) * INITIAL_FUNCTION_SIZE);

  while (exp_lst) {
    MalType *exp = exp_lst->data;
    snprintf(exp_i, INITIAL_FUNCTION_SIZE, "%s", exp->value.mal_string);
    strcat(exps, exp_i);
    exp_lst = exp_lst->next;
  }

  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "/* do - start */\n"
           "%s"
           "/* do - end */\n",
           exps);

    return make_string(code);
}

MalType *compile_letstar(MalType *expr, Env *env)
{
  return make_error("Compiler: 'let*' not implemented");
}

MalType *compile_if(MalType *expr, Env *env)
{
  List *lst = expr->value.mal_list;

  /* compile the condition expression */
  MalType *condition = compile_expression(lst->next->data, env);
  if (is_error(condition)) { return condition; }

  /* compile the 'true' branch */
  MalType *true_branch = compile_expression(lst->next->next->data, env);
  if (is_error(true_branch)) { return true_branch; }

  /* compile the (optional) 'false' branch */
  MalType *false_branch = NULL;
  if (lst->next->next->next) {
    false_branch = compile_expression(lst->next->next->next->data, env);
    if (is_error(false_branch)) { return false_branch; }
  }

  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "/* if condition - start */\n"
           "%1$s"
           "MalType *cond = result;\n"
           "/* if condition - end */\n"
           "    if (!is_false(cond) && !is_nil(cond)) {\n"
           "/* if true branch - start */\n"
           "       %2$s;\n"
           "/* if true branch - end */\n"
           "/* if false branch - start */\n"
           "    } else {\n"
           "       %3$s;\n"
           "/* if false branch - end */\n"
           "    }\n",
           condition->value.mal_string,
           true_branch->value.mal_string,
           !false_branch ? "result = make_nil()" : false_branch->value.mal_string);

  return make_string(code);
}

/* compilation primitives */

MalType *compile_symbol(MalType *expr, Env *env)
{
  /* get the value from the environment */
  char *sym = expr->value.mal_symbol;
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);

  /* assigns the symbol value to variable 'result' */
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "result = env_get(closure_env, make_symbol(\"%s\"));\n", sym);

  return make_string(code);
}

MalType *compile_nil(MalType *expr, Env *env)
{
  /* assigns 'nil' to the variable 'result' */
  return make_string("result = make_nil();");
}

MalType *compile_true(MalType *expr, Env *env)
{
  /* assigns 'trure' to the variable 'result' */
  return make_string("result = make_true();");
}

MalType *compile_false(MalType *expr, Env *env)
{
  /* assigns 'false' to the variable 'result' */
  return make_string("result = make_false();");
}

MalType *compile_integer(MalType *expr, Env *env)
{
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);

  /* assigns the integer to the variable 'result' */
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "result = make_integer(%li);\n",
           expr->value.mal_integer);

  return make_string(code);
}

MalType *compile_string(MalType *expr, Env *env)
{
  /* assigns the string to the variable 'result' */
  char *code = GC_MALLOC(sizeof(*code) * INITIAL_FUNCTION_SIZE);

  /* assigns the string to the variable 'result' */
  snprintf(code, INITIAL_FUNCTION_SIZE,
           "result = make_string(\"%s\");\n",
           expr->value.mal_string);

  return make_string(code);
}
