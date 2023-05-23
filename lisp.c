#include "mpc.h"

static char buffer[2048];


struct lisp_val;
struct lisp_env;
typedef struct lisp_val lisp_val;
typedef struct lisp_env lisp_env;
typedef lisp_val*(*lisp_builtin)(lisp_env*, lisp_val*);
// a lisp "value"
struct lisp_val {
    int type; 
    long num;
    char* err;
    char* symbol;
    char* string;
    lisp_builtin builtin;
    lisp_env* env;
    lisp_val* formals;
    lisp_val* body;
    int count;
    struct lisp_val** cell;
};

struct lisp_env {
    lisp_env* parent;
    int count;
    char** symbols;
    lisp_val** lisp_vals;
};


enum { LISP_VAL_NUM, LISP_VAL_ERR, LISP_VAL_SYMBOL, 
       LISP_VAL_SEXPR, LISP_VAL_QEXPR, LISP_VAL_FUNC. LISP_VAL_STRING};
enum { ERROR_DIV_ZERO, ERROR_BAD_OP, ERROR_BAD_NUM };

//macro
#define LASSERT(args, cond, err, ...) \
  if (!(cond)) { free_lisp_val(args); return create_lv_err(err, ##__VA_ARGS__); }

// method to create a lisp number
lisp_val* create_lv_num(long x) {
    lisp_val* value = malloc(sizeof(lisp_val));
    value->type = LISP_VAL_NUM;
    value->num = x;
    return value;
}

// method to create a lisp error
lisp_val* create_lv_err(char* msg, ...) {
    lisp_val* value = malloc(sizeof(lisp_val));
    value->type = LISP_VAL_ERR;
    va_list va;
    va_start(va, msg);

    value-> err = malloc(512);
    vsnprintf(value->err, 511, msg, va);

    value->err = realloc(value->err, strlen(value->err + 1));

    va_end(va);
    return value;
}

// method to create a lisp symbol
lisp_val* create_lv_symbol(char* s) {
  lisp_val* v = malloc(sizeof(lisp_val));
  v->type = LISP_VAL_SYMBOL;
  v->symbol = malloc(strlen(s) + 1);
  strcpy(v->symbol, s);
  return v;
}

// method to create a lisp S-Expression
lisp_val* create_lv_sexpr() {
  lisp_val* v = malloc(sizeof(lisp_val));
  v->type = LISP_VAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// method to create a lisp Q-Expression
lisp_val* create_lv_qexpr() {
    lisp_val* v = malloc(sizeof(lisp_val));
    v->type = LISP_VAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

//method to create a lisp builtin function
lisp_val* create_lv_func(lisp_builtin func) {
    lisp_val* v = malloc(sizeof(lisp_val));
    v->type = LISP_VAL_FUNC;
    v->builtin = func;
    return v;
}

//method to create a lisp env
lisp_env* create_lisp_env() {
    lisp_env* e = malloc(sizeof(lisp_env));
    e->count = 0;
    e->symbols = NULL;
    e->lisp_vals = NULL;
    e->parent = NULL;
    return e;
}

//method to create lisp lambda
lisp_val* create_lv_lambda(lisp_val* formals, lisp_val* body) {
    lisp_val* v = malloc(sizeof(lisp_val));
    v->type = LISP_VAL_FUNC;
    v->builtin = NULL;
    v->env = create_lisp_env();
    v->formals = formals;
    v->body = body;
    return v;
}

//method to create lisp string
lisp_val* create_lv_string(char* string) {
    lisp_val* v = malloc(sizeof(lisp_val));
    v->type = LISP_VAL_STRING;
    v->string = malloc(strlen(string) + 1);
    strcpy(v->string, string);
    return v;
}

void free_lisp_val(lisp_val* v);

// method to free lisp env
void free_lisp_env(lisp_env* e) {
    for(int i = 0; i < e->count; i++) {
        free(e->symbols[i]);
        free_lisp_val(e->lisp_vals[i]);
    }
    free(e->symbols);
    free(e->lisp_vals);
    free(e);
}

lisp_val* lisp_val_copy(lisp_val* v);

// get lisp env value
lisp_val* lisp_env_get(lisp_env* e, lisp_val* k) {

    // iterate over all items in env, return copy of matching record
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->symbols[i], k->symbol) == 0) {
            return lisp_val_copy(e->lisp_vals[i]);
        }
    }
    // does parent exist? if so use it
    if(e->parent) {
        return lisp_env_get(e->parent, k);
    }
    return create_lv_err("Symbol '%s' does not exist!", k->symbol);
}

// put lisp env value
void lisp_env_put(lisp_env* e, lisp_val* k, lisp_val* v) {
    // see if already exists
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->symbols[i], k->symbol) == 0) {
            free_lisp_val(e->lisp_vals[i]);
            e->lisp_vals[i] = lisp_val_copy(v);
            return;
        }
    }

    // didn't find already existing, so allocate space for new entry and copy in
    e->count++;
    e->lisp_vals = realloc(e->lisp_vals, sizeof(lisp_val*) * e->count);
    e->symbols = realloc(e->symbols, sizeof(char*) * e->count);

    e->lisp_vals[e->count - 1] = lisp_val_copy(v);
    e->symbols[e->count - 1] = malloc(strlen(k->symbol) + 1);
    strcpy(e->symbols[e->count - 1], k->symbol);
}

// define variable globally
void lisp_env_def(lisp_env* e, lisp_val* k, lisp_val* v) {
    while(e->parent) {
        e = e->parent;
    }
    lisp_env_put(e, k, v);
}

// ensure that lisp num can be converted to long; if not, error. return pointer to lisp val
lisp_val* lisp_val_read_num(mpc_ast_t* t) {
    int err = 0;
    long x = strtol(t->contents, NULL, 10);
    return err != ERANGE ?
        create_lv_num(x) : create_lv_err("Invalid number %s!", t->contents);
}

// append that lisp val to this lisp val. 
lisp_val* lisp_val_add(lisp_val* orig, lisp_val* add) {
    orig->count++;
    // adding to array, thus we have to reallocate memory to add more
    orig->cell = realloc(orig->cell, sizeof(lisp_val*) * orig->count);
    orig->cell[orig->count - 1] = add;
    return orig;
}

//append that lisp val to this lisp val, but at the head
lisp_val* lisp_val_add_at_head(lisp_val* orig, lisp_val* add) {
    orig->count++;
    // adding to array, thus we have to reallocate memory to add more
    orig->cell = realloc(orig->cell, sizeof(lisp_val*) * orig->count);
    // move all array elems up by 1
    for (int i = orig->count - 1; i > 0; i--) {
        orig->cell[i] = orig->cell[i - 1];
    }
    orig->cell[0] = add;
    return orig;
}

// read lisp val string
lisp_val* lisp_val_read_string(mpc_ast_t* t) {
    t->contents[strlen(t->contents)-1] = '\0';
    char* unescaped = malloc(strlen(t->contents+1)+1);
    strcpy(unescaped, t->contents+1);
    unescaped = mpcf_unescape(unescaped);
    lisp_val* str = create_lv_string(unescaped);
    free(unescaped);
    return str;
}

//'read' a lisp val, based on the AST created from user input:
// number --> return num lisp val
// symbol --> return symbol lisp val 
// s-expression --> return sexpr lisp val containing its children
lisp_val* lisp_val_read(mpc_ast_t* t) {

    if (strstr(t->tag, "number")) { return lisp_val_read_num(t); }
    if (strstr(t->tag, "symbol")) { return create_lv_symbol(t->contents); }
    if(strstr(t->tag, "string")) { return lisp_val_read_string(t); }

    lisp_val* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = create_lv_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = create_lv_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = create_lv_qexpr(); }
    // the lisp val is a s-expression. add the children to the lisp val, then return
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lisp_val_add(x, lisp_val_read(t->children[i]));
    }
    return x;
}

void lisp_val_print(lisp_val* v);

// print lisp val expression
void lisp_val_expr_print(lisp_val* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    lisp_val_print(v->cell[i]);

    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

// print lisp val string
void print_lisp_val_string(lisp_val* v) {
    char* mpc_escaped_string = malloc(strlen(v->string) + 1);
    strcpy(mpc_escaped_string, v->string);
    mpc_escaped_string = mpcf_escape(mpc_escaped_string);
    printf("\"%s\"", mpc_escaped_string);
    free(mpc_escaped_string);
}

// print lisp value depending on its contents
void lisp_val_print(lisp_val* v) {
  switch (v->type) {
    case LISP_VAL_NUM:   printf("%li", v->num); break;
    case LISP_VAL_STRING: print_lisp_val_string(v); break;
    case LISP_VAL_FUNC: 
        if(v->builtin) {
        printf("<builtin>");
        }
        else {
            printf("(\\ "); 
            lisp_val_print(v->formals);
            printf(" ");
            lisp_val_print(v->body);
            printf(")");
        }
        break;
    case LISP_VAL_ERR:   printf("Error: %s", v->err); break;
    case LISP_VAL_SYMBOL:printf("%s", v->symbol); break;
    case LISP_VAL_SEXPR: lisp_val_expr_print(v, '(', ')'); break;
    case LISP_VAL_QEXPR: lisp_val_expr_print(v, '{', '}'); break;
  }
}


// lisp vals are malloc'ed, so ensure that they are fully freed from the heap
void free_lisp_val(lisp_val* v) {

    switch (v->type) {

        // if num or func, stack only so no free necessary
        case LISP_VAL_NUM: break;
        case LISP_VAL_STRING: free(v->string); break;
        case LISP_VAL_FUNC: 
            if(!v->builtin) {
                free_lisp_env(v->env);
                free_lisp_val(v->formals);
                free_lisp_val(v->body);
            } 
            break;
        
        // if err/symbol, free the error or symbol
        case LISP_VAL_ERR: free(v->err); break;
        case LISP_VAL_SYMBOL: free(v->symbol); break;

        // if s-expression or q-expression, free its children
        case LISP_VAL_QEXPR:
        case LISP_VAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                free_lisp_val(v->cell[i]);
            }
            free(v->cell);
            break;
        

    }
    // now that everything necessary is freed, free the actual lisp val on heap
    free(v);
}

// extract input from user
char* readline(char* prompt) {
  fputs(prompt, stdout);
  char *line = NULL;
  size_t len = 0;
  ssize_t lineSize = 0;
  lineSize = getline(&line, &len, stdin);
  return line;
}

lisp_env* lisp_env_copy(lisp_env* e); 

// copy lisp val
lisp_val* lisp_val_copy(lisp_val* v) {

  lisp_val* x = malloc(sizeof(lisp_val));
  x->type = v->type;

  switch (v->type) {

    case LISP_VAL_FUNC: 
        if(v->builtin) {
            x->builtin = v->builtin;
        }
        else {
            x->builtin = NULL;
            x->env = lisp_env_copy(v->env);
            x->formals = lisp_val_copy(v->formals);
            x->body = lisp_val_copy(v->body);
        }
        break;

    case LISP_VAL_NUM: x->num = v->num; break;
    case LISP_VAL_STRING: x->string = malloc(strlen(v->string) + 1); strcpy(x->string, v->string); break;

    case LISP_VAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LISP_VAL_SYMBOL:
      x->symbol = malloc(strlen(v->symbol) + 1);
      strcpy(x->symbol, v->symbol); break;

    case LISP_VAL_SEXPR:
    case LISP_VAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lisp_val*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lisp_val_copy(v->cell[i]);
      }
    break;
  }

  return x;
}

//copy lisp env
lisp_env* lisp_env_copy(lisp_env* e) {
    lisp_env* new = malloc(sizeof(lisp_env));
    new->parent = e->parent;
    new->count = e->count;
    new->symbols = malloc(sizeof(char*) * new->count);
    new->lisp_vals = malloc(sizeof(lisp_val*) * new->count);
    for(int i = 0; i < e->count; i++) {
        new->symbols[i] = malloc(strlen(e->symbols[i]) + 1);
        strcpy(new->symbols[i], e->symbols[i]);
        new->lisp_vals[i] = lisp_val_copy(e->lisp_vals[i]);
    }
    return new;
}

lisp_val* lisp_val_pop(lisp_val* v, int i);
lisp_val* builtin_eval(lisp_env* e, lisp_val* v);

lisp_val* builtin_list(lisp_env* e, lisp_val* v); 

// call function
lisp_val* lisp_val_call(lisp_env* e, lisp_val* f, lisp_val* v) {
    if(f->builtin) {
        return f->builtin(e, v);
    }

    int count = v->count;
    int total = f->formals->count;

    while(v->count) {
        if(f->formals->count == 0) {
            free_lisp_val(v);
            return create_lv_err(
        "Function passed too many arguments. "
        "Got %i, Expected %i.", count, total);
        }
        lisp_val* symbol = lisp_val_pop(f->formals, 0);
        if (strcmp(symbol->symbol, "&") == 0) {
            if (f->formals->count != 1) {
                free_lisp_val(v);
                return create_lv_err("Function format invalid. "
                  "Symbol '&' not followed by single symbol.");
              }
            
            lisp_val* nsym = lisp_val_pop(f->formals, 0);
            lisp_env_put(f->env, nsym, builtin_list(e, v));
            free_lisp_val(symbol);
            free_lisp_val(nsym);
            break;
        }
        lisp_val* val = lisp_val_pop(v, 0);
        lisp_env_put(f->env, symbol, val);
        free_lisp_val(symbol);
        free_lisp_val(val);
    }
    free_lisp_val(v);
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->symbol, "&") == 0) {
        if (f->formals->count != 2) {
            return create_lv_err("Function format invalid. Symbol '&' not followed by single symbol.");
        }
      
        free_lisp_val(lisp_val_pop(f->formals, 0));
      
        lisp_val* symbol = lisp_val_pop(f->formals, 0);
        lisp_val* val = create_lv_qexpr();
      
        lisp_env_put(f->env, symbol, val);
        free_lisp_val(symbol); 
        free_lisp_val(val);
    }
    if(f->formals->count == 0) {
        f->env->parent = e;
        return builtin_eval(f->env, lisp_val_add(create_lv_sexpr(), lisp_val_copy(f->body)));
    }
    // return partially evaluated function
    return lisp_val_copy(f);
}

// take two lisp vals + operator and return the result of the operation
lisp_val* evaluate_op(lisp_val x1, char* operator, lisp_val x2) {
    
    if (strcmp(operator, "+") == 0) { return create_lv_num(x1.num + x2.num); }
    if (strcmp(operator, "-") == 0) { return create_lv_num(x1.num - x2.num); }
    if (strcmp(operator, "*") == 0) { return create_lv_num(x1.num * x2.num); }
    if (strcmp(operator, "/") == 0) {
    /* If second operand is zero return error */
    return x2.num == 0 
      ? create_lv_err("Divided by zero!") : create_lv_num(x1.num / x2.num);
  }
    if (strcmp(operator, "%") == 0) { return create_lv_num(x1.num % x2.num); }
    if (strcmp(operator, "^") == 0) { return create_lv_num(pow(x1.num, x2.num)); }
    return create_lv_err("Bad operation %s!", operator);

}

// take child i from lisp val cells, remove from cells, return it.
lisp_val* lisp_val_pop(lisp_val* v, int i) {

    lisp_val* x = v->cell[i];

    // shift memory 
    memmove(&v->cell[i], &v->cell[i+1],
      sizeof(lisp_val*) * (v->count-i-1));

    v->count--;

    // re-allocate memory
    v->cell = realloc(v->cell, sizeof(lisp_val*) * v->count);
    return x;
}

// take child i from lisp val cells, then wipe the lisp val
lisp_val* lisp_val_take(lisp_val* v, int i) {
    lisp_val* x = lisp_val_pop(v, i);
    free_lisp_val(v);
    return x;
}

// take head of q-expr
lisp_val* builtin_head(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 1, "'head' takes only 1 argument. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "Cannot take 'head' of non-q-expression.");
    LASSERT(v, v->cell[1]->count != 0, "'head' passed empty q-expression");

    lisp_val* lv = lisp_val_take(v, 0);
    while(lv->count > 1) {
        free_lisp_val(lisp_val_pop(lv, 1));
    }
    return lv;
}

// take tail of q-expr
lisp_val* builtin_tail(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 1, "'tail' takes only 1 argument. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "Cannot take 'tail' of non-q-expression.");
    LASSERT(v, v->cell[1]->count != 0, "'tail' passed empty q-expression");

    lisp_val* lv = lisp_val_take(v, 0);
    free_lisp_val(lisp_val_pop(lv, 0));
    return lv;
}

// convert s-expr to q-expr
lisp_val* builtin_list(lisp_env* e, lisp_val* v) {
    v->type = LISP_VAL_QEXPR;
    return v;
}

// takes a value and a Q-Expression and appends it to the front
lisp_val* builtin_cons(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 2, "'cons' takes exactly 2 arguments. Got %i", v->count);
    LASSERT(v, v->cell[1]->type == LISP_VAL_QEXPR,
            "'cons' requires the second parameter to be a q-expression.");
    LASSERT(v, v->cell[1]->count != 0, "'cons' passed empty q-expression");

    lisp_val* lv2 = lisp_val_pop(v, 1);
    lisp_val* lv1 = lisp_val_take(v, 0);

    lv2 = lisp_val_add_at_head(lv2, lv1);
    return lv2;
}

// returns length of q expression
lisp_val* builtin_len(lisp_env* e, lisp_val* v) {

    LASSERT(v, v->count == 1, "'len' takes only 1 argument. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "Cannot take 'len' of non-q-expression.");

    lisp_val* lv = lisp_val_take(v, 0);

    return create_lv_num(lv->count);

}

// takes a q-expression and returns all of it except last element
lisp_val* builtin_init(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 1, "'init' takes only 1 argument. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "Cannot take 'init' of non-q-expression.");
    LASSERT(v, v->cell[1]->count != 0, "'init' passed empty q-expression");

    lisp_val* lv = lisp_val_take(v, 0);

    lisp_val_pop(lv, lv->count - 1);
    return lv;
}

lisp_val* lisp_val_eval(lisp_env* e, lisp_val* v);

// change q-expr to s-expr and evaluate
lisp_val* builtin_eval(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 1, "'eval' takes only 1 argument. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "Cannot take 'eval' of non-q-expression.");

    lisp_val* lv = lisp_val_take(v, 0);
    lv->type = LISP_VAL_SEXPR;

    return lisp_val_eval(e, lv);

}

// join multiple lisp vals
lisp_val* lisp_val_join(lisp_val* v1, lisp_val* v2) {
    while(v2->count > 0) {
        v1 = lisp_val_add(v1, lisp_val_pop(v2, 0));
    }

    free_lisp_val(v2);
    
    return v1;
}

// join multiple q-exprs
lisp_val* builtin_join(lisp_env* e, lisp_val* v) {
    for (int i = 0; i < v->count; i++) {
        LASSERT(v, v->cell[i]->type == LISP_VAL_QEXPR,"'join' passed non-q-expression.");
    }
    lisp_val* lv = lisp_val_pop(v, 0);
    
    while(v->count > 0) {
        lv = lisp_val_join(lv, lisp_val_pop(v, 0));
    }
    
    free_lisp_val(v);
    return lv;
}

lisp_val* builtin_var(lisp_env* e, lisp_val* v, char* func) {
    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "'%s' must be q-expression", func);

    // 1st arg -- list of symbols
    lisp_val* symbols = v->cell[0];

    // if not all are symbols, error out
    for (int i = 0; i < symbols->count; i++) {
        LASSERT(v, symbols->cell[i]->type == LISP_VAL_SYMBOL, "Arguments must be symbols!");
    }

    LASSERT(v, v->count - 1 == symbols->count, 
            "'%s' argument mismatch: there must be a value for each symbol. \
            Values: %i, Symbols: %i", func, v->count - 1, symbols->count);

    for(int i = 0; i < symbols->count; i++) {
        if(strcmp(func, "=") == 0) {
            lisp_env_put(e, symbols->cell[i], v->cell[i+1]);
        }

        if(strcmp(func, "def") == 0) {
            lisp_env_def(e, symbols->cell[i], v->cell[i+1]);
        }
    }

    free_lisp_val(v);
    // on success print ()
    return create_lv_sexpr();
}

lisp_val* builtin_def(lisp_env* e, lisp_val* v) {
    return builtin_var(e, v, "def");
}

lisp_val* builtin_put(lisp_env* e, lisp_val* v) {
    return builtin_var(e, v, "=");
}

lisp_val* builtin_lambda(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 2, "'lambda' takes exactly two arguments.");

    LASSERT(v, v->cell[0]->type == LISP_VAL_QEXPR, "'lambda' must use q-expression for argument 1");
    LASSERT(v, v->cell[1]->type == LISP_VAL_QEXPR, "'lambda' must use q-expression for argument 2");

    for(int i = 0; i < v->cell[0]->count; i++) {
        LASSERT(v, v->cell[0]->cell[i]->type == LISP_VAL_SYMBOL, "Cannot define non-symbol."); 
    }
    lisp_val* formals = lisp_val_pop(v, 0);
    lisp_val* body = lisp_val_pop(v, 0);
    free_lisp_val(v);

    return create_lv_lambda(formals, body);
}

// perform operation on lisp val
lisp_val* builtin_op(lisp_env* e, lisp_val* a, char* op) {

    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LISP_VAL_NUM) {
            free_lisp_val(a);
            return create_lv_err("Operation must be done on numbers.");
        }
    }

    lisp_val* x = lisp_val_pop(a, 0);

    // (- x) --> -x
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // run over all elements
    while (a->count > 0) {

      // get next elem
        lisp_val* y = lisp_val_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                free_lisp_val(x); free_lisp_val(y);
                x = create_lv_err("Division by zero error.");
                break;
            }
            x->num /= y->num;
        }
        free_lisp_val(y);
    }

    free_lisp_val(a);
    return x;
}

//lisp_val* builtin(lisp_val* a, char* op) {
//    if (strcmp("list", op) == 0) { return builtin_list(a); }
//    if (strcmp("head", op) == 0) { return builtin_head(a); }
//    if (strcmp("tail", op) == 0) { return builtin_tail(a); }
//    if (strcmp("join", op) == 0) { return builtin_join(a); }
//    if (strcmp("eval", op) == 0) { return builtin_eval(a); }
//    if (strcmp("cons", op) == 0) { return builtin_cons(a); }
//    if (strcmp("init", op) == 0) { return builtin_init(a); }
//    if (strcmp("len", op) == 0) { return builtin_len(a); }
//    if (strstr("+-/*^%", op)) { return builtin_op(a, op); }
//    free_lisp_val(a);
//    return create_lv_err("Unknown Function!");
//}

lisp_val* builtin_add(lisp_env* e, lisp_val* v) {
    return builtin_op(e, v, "+");
}


lisp_val* builtin_sub(lisp_env* e, lisp_val* v) {
    return builtin_op(e, v, "-");
}


lisp_val* builtin_mul(lisp_env* e, lisp_val* v) {
    return builtin_op(e, v, "*");
}


lisp_val* builtin_div(lisp_env* e, lisp_val* v) {
    return builtin_op(e, v, "/");
}

int lisp_val_equals(lisp_val* x1, lisp_val* x2) {
    if(x1->type != x2->type) {
        return 0; // types are not equal
    }
    switch(x1->type) {
        case LISP_VAL_NUM:    return x1->num == x2->num;
        case LISP_VAL_STRING: return strcmp(x1->string, x2->string) == 0;
        case LISP_VAL_ERR:    return strcmp(x1->err, x2->err) == 0;
        case LISP_VAL_SYMBOL: return strcmp(x1->symbol, x2->symbol) == 0;
        case LISP_VAL_QEXPR:
        case LISP_VAL_SEXPR:
                              if(x1->count != x2->count) { return 0; }
                              for(int i = 0; i < x1->count; i++) {
                                  if(!lisp_val_equals(x1->cell[i], x2->cell[i])) { return 0; }
                              }
                              return 1;
                              break;
        case LISP_VAL_FUNC: 
                              if(x1->builtin || x2->builtin) {
                                  return x1->builtin == x2->builtin;
                              }
                              return lisp_val_equals(x1->formals, x2->formals)
                                  && lisp_val_equals(x1->body   , x2->body   );
    }
}

lisp_val* builtin_order(lisp_env* e, lisp_val* v, char* op) {
    int result;
    if(strcmp(op, ">") == 0) {
        result = v->cell[0]->num > v->cell[1]->num;
    }
    else if(strcmp(op, "<") == 0) {
        result = v->cell[0]->num < v->cell[1]->num;
    }
    else if(strcmp(op, ">=") == 0) {
        result = v->cell[0]->num >= v->cell[1]->num;
    }
    else if(strcmp(op, "<=") == 0) {
        result = v->cell[0]->num <= v->cell[1]->num;
    }
    free_lisp_val(v);
    return create_lv_num(result);
}

lisp_val* builtin_gt(lisp_env* e, lisp_val* v) {
    return builtin_order(e, v, ">");
}

lisp_val* builtin_gte(lisp_env* e, lisp_val* v) {
    return builtin_order(e, v, ">=");
}

lisp_val* builtin_lt(lisp_env* e, lisp_val* v) {
    return builtin_order(e, v, "<");
}

lisp_val* builtin_lte(lisp_env* e, lisp_val* v) {
    return builtin_order(e, v, "<=");
}

lisp_val* builtin_compare(lisp_env* e, lisp_val* v, char* op) {
    LASSERT(v, v->count == 2, "'%s' takes only 2 arguments. Got %i", op, v->count);
    int result;
    if(strcmp(op, "==") == 0) {
        result = lisp_val_equals(v->cell[0], v->cell[1]);
    }
    else if(strcmp(op, "!=") == 0) {
        result = !lisp_val_equals(v->cell[0], v->cell[1]);
    }
    free_lisp_val(v);
    return create_lv_num(result);
}

lisp_val* builtin_eq(lisp_env* e, lisp_val* v) {
    return builtin_compare(e, v, "==");
}

lisp_val* builtin_neq(lisp_env* e, lisp_val* v) {
    return builtin_compare(e, v, "!=");
}

lisp_val* builtin_if(lisp_env* e, lisp_val* v) {
    LASSERT(v, v->count == 3, "'if' takes 3 arguments. Got %i", v->count);
    LASSERT(v, v->cell[0]->type == LISP_VAL_NUM, "Argument 1 of 'if' must be bool");
    LASSERT(v, v->cell[1]->type == LISP_VAL_QEXPR, "Argument 1 of 'if' must be q-expression");
    LASSERT(v, v->cell[2]->type == LISP_VAL_QEXPR, "Argument 2 of 'if' must be q-expression");

    lisp_val* result;

    v->cell[1]->type = LISP_VAL_SEXPR;
    v->cell[2]->type = LISP_VAL_SEXPR;
    if(v->cell[0]->num) {
        result = lisp_val_eval(e, lisp_val_pop(v, 1));
    }
    else {
        result = lisp_val_eval(e, lisp_val_pop(v, 2));
    }

    free_lisp_val(v);
    
    return result;

}

void lisp_env_add_builtin(lisp_env* e, char* name, lisp_builtin func) {
    lisp_val* k = create_lv_symbol(name);
    lisp_val* v = create_lv_func(func);
    lisp_env_put(e, k, v);
    free_lisp_val(k);
    free_lisp_val(v);
}

void lisp_env_add_builtins(lisp_env* e) {

    lisp_env_add_builtin(e, "list", builtin_list);
    lisp_env_add_builtin(e, "head", builtin_head);
    lisp_env_add_builtin(e, "tail", builtin_tail);
    lisp_env_add_builtin(e, "eval", builtin_eval);
    lisp_env_add_builtin(e, "join", builtin_join);

    lisp_env_add_builtin(e, "+", builtin_add);
    lisp_env_add_builtin(e, "-", builtin_sub);
    lisp_env_add_builtin(e, "*", builtin_mul);
    lisp_env_add_builtin(e, "/", builtin_div);

    lisp_env_add_builtin(e, "def", builtin_def);
    lisp_env_add_builtin(e, "\\", builtin_lambda);
    lisp_env_add_builtin(e, "=", builtin_put);

    lisp_env_add_builtin(e, "==", builtin_eq);
    lisp_env_add_builtin(e, "!=", builtin_neq);
    lisp_env_add_builtin(e, ">",  builtin_gt);
    lisp_env_add_builtin(e, "<",  builtin_lt);
    lisp_env_add_builtin(e, ">=", builtin_gte);
    lisp_env_add_builtin(e, "<=", builtin_lte);
    lisp_env_add_builtin(e, "if", builtin_if);
}

lisp_val* lisp_val_eval_sexpr(lisp_env* e, lisp_val* v);

// evaluate lisp val.
lisp_val* lisp_val_eval(lisp_env* e, lisp_val* v) {
    if(v->type == LISP_VAL_SYMBOL) {
        lisp_val* x = lisp_env_get(e, v);
        free_lisp_val(v);
        return x;
    }
    if (v->type == LISP_VAL_SEXPR) { return lisp_val_eval_sexpr(e, v); }
    //if not lisp val, the value is just itself
    return v;
}

// evaluate lisp val if it is a s-expression
lisp_val* lisp_val_eval_sexpr(lisp_env* e, lisp_val* v) {

    // evaluate each cell
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lisp_val_eval(e, v->cell[i]);
    }

    // if there is an error, take the error and wipe away the rest of the lisp val
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LISP_VAL_ERR) { return lisp_val_take(v, i); }
    }

    // no cells
    if (v->count == 0) { return v; }

    // only one cell, so just take first child
    if (v->count == 1) { return lisp_val_take(v, 0); }

    lisp_val* f = lisp_val_pop(v, 0);
    if(f->type != LISP_VAL_FUNC) {
        free_lisp_val(v);
        free_lisp_val(f);
        return create_lv_err("First element is not a function!");
    }

    lisp_val* result = lisp_val_call(e, f, v);
    free_lisp_val(f);
    return result;
}


//main method, asks for user input and returns result
int main(int argc, char** argv) {

    // MPC library used for AST creation. For more info, see buildyourownlisp(dot)com
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");
    
    mpca_lang(MPCA_LANG_DEFAULT,
      "                                                      \
        number : /-?[0-9]+/ ;                                \
        string  : /\"(\\\\.|[^\"])*\"/ ;                     \
        symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;          \
        sexpr  : '(' <expr>* ')' ;                           \
        qexpr  : '{' <expr>* '}' ;                           \
        expr   : <number> | <symbol> | <sexpr> | <qexpr>;    \
        lispy  : /^/ <expr>* /$/ ;                           \
      ",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  printf("Clisp terminal\r\n");
  printf("Type 'exit' to exit, or ctrl-c.\r\n");
  lisp_env* e = create_lisp_env();
  lisp_env_add_builtins(e);
  
  while (1) {
  
    char* input = readline("clisp> ");
    char* ex = input;
    ex[strlen(ex) - 1] = '\0';
    if(strcmp(ex, "exit") == 0) {
        printf("Later!\r\n");
        exit(0);
    }
    // parse user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
        lisp_val* lv = lisp_val_eval(e, lisp_val_read(r.output));
        lisp_val_print(lv);
        printf("\r\n");
        free_lisp_val(lv);
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
    
    free(input);
  }
  
  // delete parsers
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  // delete environment
  free_lisp_env(e);
  
  return 0;
}

