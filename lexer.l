%{
    // Analizador Léxico

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "parser.tab.h"

    int lexical_errors = 0;

    // Línea del último token y del anterior (para reportar errores sintácticos)
    int cur_line = 1;
    int prev_line = 1;

    // Se ejecuta en cada match; ignora espacios y comentarios
    #define YY_USER_ACTION \
        yylloc.first_line = yylineno; \
        if (!(yytext[0] == ' ' || yytext[0] == '\t' || yytext[0] == '\n' || \
              (yyleng >= 2 && yytext[0] == '/' && (yytext[1] == '/' || yytext[1] == '*')))) { \
            prev_line = cur_line; \
            cur_line = yylineno; \
        }
%}

%option yylineno

%%
    /* Palabras reservadas */

"boolean"   { return BOOLEAN; }
"else"      { return ELSE; }
"extern"    { return EXTERN; }
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
"/*"        {   /* solo matchea si el comentario no se cierra (la regla anterior es más larga) */
                int linea = yylineno;
                int c;
                while ((c = input()) != 0 && c != EOF);
                lexical_errors++;
                fprintf(stderr, "Error léxico: comentario sin cerrar en la línea %d\n", linea); }
[ \t]+      { /* ignorar */ }
\n          { /* ignorar */ }

    /* Identificadores */

[a-zA-Z][a-zA-Z0-9_]* { yylval.id = strdup(yytext); return ID; }

    /* Literal entero */

[0-9]+      {   unsigned long long valor = strtoull(yytext, NULL, 10);
                if (valor > 2147483648ULL) {
                    lexical_errors++;
                    fprintf(stderr, "Error léxico: entero '%s' fuera de rango en la línea %d\n", yytext, yylineno);
                } else if (valor == 2147483648ULL) {
                    /* 2147483648 solo es válido como -2147483648 (INT_MIN).
                       El parser/AST deciden si está negado. */
                    return CONSTINT_MIN;
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
int yywrap() { prev_line = cur_line; return 1; }