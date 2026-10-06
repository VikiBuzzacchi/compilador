%{
// Analizador Sintáctico

#include <stdio.h>
#include "ast.h"
#include "symbol_table.h"

extern int yylex();
extern int lexical_errors;
extern int prev_line;
extern int cur_line;
void yyerror(const char *s);
struct node *root;
int syntax_errors = 0;

// Declara un símbolo en el nivel actual. Si el nombre ya existe ahí, reporta el error
static struct symbol *declare(char *name, enum sym_type type, enum sym_kind kind) {
    struct symbol *s = insert_symbol(name, type, kind);
    if (!s) {
        semantic_errors++;
        fprintf(stderr, "Error semántico en la línea %d: '%s' ya declarado\n", cur_line, name);
    }
    return s;
}

// Nodo de declaración de un parámetro: se declara en el nivel del método
static struct node *param_node(char *name, enum sym_type type) {
    struct node *n = new_var_decl(declare(name, type, SYM_PARAM), cur_line);
    n->id = name;
    n->data_type = type;
    return n;
}

// Nodo de declaración sin resolver: id_list no conoce el tipo, lo recibe var_decl
static struct node *name_node(char *name) {
    struct node *n = new_var_decl(NULL, cur_line);
    n->id = name;
    return n;
}
%}

%union {
    int val;
    float fval;
    char *id;
    int type;               /* Es el tipo registrado en el txt para la declaración*/
    struct symbol *sym;
    struct node *ast;
}

%token INT BOOLEAN TRUE FALSE
%token <id> ID
%token <val> CONSTINT
%token <fval> CONSTFLOAT
%token RETURN VOID
%token ELSE IF WHILE FLOAT
%token OR_OP AND_OP EQ_OP


%type <type> type
%type <sym> method_head
%type <ast> var_decl_list var_decl id_list method_decl_list method_decl
%type <ast> param_list param_list_ne block block_item_list block_item statement
%type <ast> method_call expr_list expr_list_ne expr literal bool_literal

/* Precedencia de operadores */
%left OR_OP
%left AND_OP
%left EQ_OP
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%right UMINUS UNOT     /* precedencia - y ! */

%define parse.error verbose

%start program

%%
program:    var_decl_list method_decl_list  { root = new_program($1, $2); }
            | method_decl_list              { root = new_program(NULL, $1); }
            ;

var_decl_list: var_decl_list var_decl   { $$ = append_node($1, $2); }
            |  var_decl                 { $$ = $1; }
            ;

var_decl: type id_list ';' {
              for (struct node *n = $2; n; n = n->next) {
                  n->sym = declare(n->id, $1, SYM_VAR);
                  n->data_type = $1;
              }
              $$ = $2;
          }
        ;

id_list: ID                 { $$ = name_node($1); }
       | ID ',' id_list     { $$ = name_node($1); $$->next = $3; }
       ;

method_decl_list: method_decl_list method_decl  { $$ = append_node($1, $2); }
                 | method_decl                  { $$ = $1; }
                 ;

/* El nivel del método se abre en method_head y se cierra acá:
   contiene los parámetros y las variables del cuerpo */
method_decl: method_head '(' param_list ')' '{' block_item_list '}' {
                 $$ = new_method_decl($1, $3, new_block($6, cur_line), cur_line);
                 close_level();
             }
           ;

method_head: type ID    { $$ = declare($2, $1, SYM_METHOD); open_level(); }
           | VOID ID    { $$ = declare($2, TYPE_VOID, SYM_METHOD); open_level(); }
           ;

param_list: param_list_ne   { $$ = $1; }
          | %empty          { $$ = NULL; }
          ;

param_list_ne: type ID                      { $$ = param_node($2, $1); }
             | type ID ',' param_list_ne    { $$ = param_node($2, $1); $$->next = $4; }
             ;

/* Cada bloque if/while abre y cierra su propio nivel */
block: '{' { open_level(); } block_item_list '}' {
           $$ = new_block($3, cur_line);
           close_level();
       }
     ;

block_item_list: block_item_list block_item     { $$ = append_node($1, $2); }
               | %empty                         { $$ = NULL; }
               ;

block_item: var_decl    { $$ = $1; }
          | statement   { $$ = $1; }
          ;

type: INT       { $$ = TYPE_INT; }
    | BOOLEAN   { $$ = TYPE_BOOL; }
    | FLOAT     { $$ = TYPE_FLOAT; }
    ;

statement: ID '=' expr ';'                  { $$ = new_assign(new_id($1, cur_line), $3, cur_line); }
         | method_call ';'                  { $$ = $1; }
         | IF '(' expr ')' block            { $$ = new_if($3, $5, NULL, cur_line); }
         | IF '(' expr ')' block ELSE block { $$ = new_if($3, $5, $7, cur_line); }
         | WHILE '(' expr ')' block         { $$ = new_while($3, $5, cur_line); }
         | RETURN expr ';'                  { $$ = new_return($2, cur_line); }
         | RETURN ';'                       { $$ = new_return(NULL, cur_line); }
         | ';'                              { $$ = NULL; }
         | block                            { $$ = $1; }
         | error ';'                        { yyerrok; $$ = NULL; }
         | ID '=' expr error                { $$ = NULL; }  /* falta el ';'. Sin yyerrok: no reporta tokens sobrantes */
         ;

method_call: ID '(' expr_list ')'   { $$ = new_call($1, $3, cur_line); }
           ;

expr_list: expr_list_ne     { $$ = $1; }
         | %empty           { $$ = NULL; }
         ;

expr_list_ne: expr                  { $$ = $1; }
            | expr ',' expr_list_ne { $$ = $1; $$->next = $3; }
            ;

expr: ID                    { $$ = new_id($1, cur_line); }
    | method_call           { $$ = $1; }
    | literal               { $$ = $1; }
    | expr OR_OP  expr      { $$ = new_binop(OP_OR,  $1, $3, cur_line); }
    | expr AND_OP expr      { $$ = new_binop(OP_AND, $1, $3, cur_line); }
    | expr EQ_OP  expr      { $$ = new_binop(OP_EQ,  $1, $3, cur_line); }
    | expr '<'    expr      { $$ = new_binop(OP_LT,  $1, $3, cur_line); }
    | expr '>'    expr      { $$ = new_binop(OP_GT,  $1, $3, cur_line); }
    | expr '+'    expr      { $$ = new_binop(OP_ADD, $1, $3, cur_line); }
    | expr '-'    expr      { $$ = new_binop(OP_SUB, $1, $3, cur_line); }
    | expr '*'    expr      { $$ = new_binop(OP_MUL, $1, $3, cur_line); }
    | expr '/'    expr      { $$ = new_binop(OP_DIV, $1, $3, cur_line); }
    | expr '%'    expr      { $$ = new_binop(OP_MOD, $1, $3, cur_line); }
    | '-' expr    %prec UMINUS  { $$ = new_unop(OP_NEG, $2, cur_line); }
    | '!' expr    %prec UNOT    { $$ = new_unop(OP_NOT, $2, cur_line); }
    | '(' expr ')'          { $$ = $2; }
    ;

literal: CONSTINT       { $$ = new_int($1, cur_line); }
       | CONSTFLOAT     { $$ = new_float($1, cur_line); }
       | bool_literal   { $$ = $1; }
       ;

bool_literal: TRUE      { $$ = new_bool(1, cur_line); }
            | FALSE     { $$ = new_bool(0, cur_line); }
            ;
%%
void yyerror(const char *s) {
    syntax_errors++;
    fprintf(stderr, "Error sintáctico en la línea %d: %s\n", prev_line, s);
}
