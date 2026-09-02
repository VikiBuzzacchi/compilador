%{
// Analizador Sintáctico

#include <stdio.h>
#include "ast.h"
#include "symbol_table.h"

extern int yylex();
extern int lexical_errors;
void yyerror(const char *s);
struct node *root;
int syntax_errors = 0;
%}

%union {
    int val;
    char *id;
    struct node *ast;
}

%token MAIN INT BOOL TRUE FALSE INVALID
%token <id> ID
%token <val> CONST
%token RETURN VOID
%type <ast> exp sent lista_sent

%start program

%left '+' '-'
%left '*'
%nonassoc UMINUS

%%
tipo_main: INT | BOOL | VOID ;

program: tipo_main MAIN '(' ')' '{' { init_symbol_table(); } lista_sent '}' { root = $7; } ;

lista_sent: sent lista_sent { $$ = create_node(NODE_SEQ, $1, $2); }
          | { $$ = NULL; } ;

sent: INT ID ';'        { insert_symbol($2, TYPE_INT); $$ = NULL; }
    | BOOL ID ';'       { insert_symbol($2, TYPE_BOOL); $$ = NULL; }
    | ID '=' exp ';'    { $$ = create_node(NODE_ASG, create_var_node($1), $3); }
    | RETURN exp ';'    { $$ = create_node(NODE_RET, $2, NULL); }
    | RETURN ';'        { $$ = create_node(NODE_RET, NULL, NULL); }
    | INVALID ';'       { syntax_errors++; yyerrok; $$ = NULL; }
    | error ';'         { syntax_errors++; yyerrok; $$ = NULL; }
    ;

exp: exp '+' exp        { $$ = create_node(NODE_ADD, $1, $3); }
   | exp '-' exp        { $$ = create_node(NODE_SUB, $1, $3); }
   | exp '*' exp        { $$ = create_node(NODE_MUL, $1, $3); }
   | '-' exp %prec UMINUS { $$ = create_node(NODE_NEG, $2, NULL); }
   | CONST              { $$ = create_const_node($1); }
   | ID                 { $$ = create_var_node($1); }
   | TRUE               { $$ = create_const_node(1); }
   | FALSE              { $$ = create_const_node(0); }
   | '(' exp ')'        { $$ = $2; }
   ;
%%
void yyerror(const char *s) {
    syntax_errors++;
    fprintf(stderr, "Error sintáctico: %s\n", s);
}