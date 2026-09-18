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
    float fval;
    char *id;
    struct node *ast;
}

%token INT BOOLEAN TRUE FALSE
%token <id> ID
%token <val> CONSTINT
%token <fval> CONSTFLOAT
%token RETURN VOID
%token ELSE IF WHILE FLOAT
%token OR_OP AND_OP EQ_OP

/* Precedencia de operadores */
%left OR_OP
%left AND_OP
%left EQ_OP
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%right UMINUS UNOT     /* precedencia - y ! */

%start program

%%
program: var_decl_list method_decl_list ;

var_decl_list: var_decl var_decl_list
             | %empty
             ;

var_decl: type id_list ';' ;

id_list: ID
       | ID ',' id_list
       ;

method_decl_list: method_decl method_decl_list
                 | %empty
                 ;

method_decl: type ID '(' param_list ')' block
           | VOID ID '(' param_list ')' block
           ;

param_list: param_list_ne
          | %empty
          ;

param_list_ne: type ID
             | type ID ',' param_list_ne
             ;

block: '{' var_decl_list statement_list '}' ;

statement_list: statement statement_list
              | %empty
              ;

type: INT
    | BOOLEAN
    | FLOAT
    ;

statement: ID '=' expr ';'
         | method_call ';'
         | IF '(' expr ')' block
         | IF '(' expr ')' block ELSE block
         | WHILE '(' expr ')' block
         | RETURN expr ';'
         | RETURN ';'
         | ';'
         | block
         | error ';'      { yyerrok; }
         ;

method_call: ID '(' expr_list ')' ;

expr_list: expr_list_ne
         | %empty
         ;

expr_list_ne: expr
            | expr ',' expr_list_ne
            ;

expr: ID
    | method_call
    | literal
    | expr OR_OP  expr
    | expr AND_OP expr
    | expr EQ_OP  expr
    | expr '<'    expr
    | expr '>'    expr
    | expr '+'    expr
    | expr '-'    expr
    | expr '*'    expr
    | expr '/'    expr
    | expr '%'    expr
    | '-' expr    %prec UMINUS
    | '!' expr    %prec UNOT
    | '(' expr ')'
    ;

literal: CONSTINT
       | CONSTFLOAT
       | bool_literal
       ;

bool_literal: TRUE
            | FALSE
            ;
%%
void yyerror(const char *s) {
    syntax_errors++;
    fprintf(stderr, "Error sintáctico: %s\n", s);
}
