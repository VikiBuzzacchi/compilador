%{
    // Analizador Léxico

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "parser.tab.h"

    int lexical_errors = 0;
%}

%option yylineno

%%
    /* Palabras reservadas */

"boolean"   { fprintf(BOOLEAN); return BOOLEAN; }
"else"      { fprintf(ELSE); return ELSE; }
"false"     { return FALSE; }
"if"        { return IF; }
"int"       { return INT; }
"return"    { return RETURN; }
"true"      { return TRUE; }
"void"      { return VOID; }
"while"     { return WHILE; }
"float"     { return FLOAT; }

    /* Comentarios y espacios */

"//".*    { /* comentario de linea */}
\/\*([^*]|\*+[^*/])*\*+\/ { /* comentario multilinea */ }
[ \t]+      { /* ignorar */ }
\n          { /* ignorar */ }

    /* Identificadores */

[a-zA-Z][a-zA-Z0-9]* { yylval.id = strdup(yytext); return ID; }

    /* Literal entero */

[0-9]+      {   long valor = strtol(yytext, NULL, 10);
                if (valor > 2147483647L || valor < -2147483648L) {
                    fprintf(stderr, "Error: entero fuera de rango: %s\n", yytext);
                }
                yylval.val = (int) valor;
                return CONSTINT; }

    /* Literal real */

[0-9]+\.[0-9]+  { yylval.fval = strtof(yytext, NULL); return CONSTFLOAT; }

[0-9]+"."[0-9]*([eE][+-]?[0-9]+)?   { yylval.fval = strtof(yytext, NULL); return CONSTFLOAT; }
[0-9]*"."[0-9]+([eE][+-]?[0-9]+)?   { yylval.fval = strtof(yytext, NULL); return CONSTFLOAT; }
[0-9]+[eE][+-]?[0-9]+               { yylval.fval = strtof(yytext, NULL); return CONSTFLOAT; }


    /* Operadores */

"||"        { return OR_OP; }
"&&"        { return AND_OP; }
"=="        { return EQ_OP; }
"="         { return '='; }
"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"/"         { return '/'; }
"%"         { return '%'; }
"!"         { return '!'; }
"<"         { return '<'; }
">"         { return '>'; }

"{"         { return '{'; }
"}"         { return '}'; }
"("         { return '('; }
")"         { return ')'; }
","         { return ','; }
";"         { return ';'; }

    /* Otro caracter */

.           { lexical_errors++; fprintf(stderr, "Error léxico: símbolo '%s' no reconocido en la línea %d\n", yytext, yylineno); }

%%
int yywrap() { return 1; }