%{
    // Analizador Léxico

    #include <stdio.h>
    #include <string.h>
    #include "parser.tab.h"

    int lexical_errors = 0;
%}

%option yylineno

%%
"main"      { return ID; }  /* main es un identificador normal en parser.y */
"int"       { return INT; }
"bool"      { return BOOLEAN; }
"float"     { return FLOAT; }
"true"      { return TRUE; }
"false"     { return FALSE; }
"return"    { return RETURN; }
"void"      { return VOID; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
[a-zA-Z][a-zA-Z0-9]* { yylval.id = strdup(yytext); return ID; }
[0-9]+\.[0-9]+ { yylval.fval = atof(yytext); return CONSTFLOAT; }
[0-9]+      { yylval.val = atoi(yytext); return CONSTINT; }
","         { return ','; }
"="         { return '='; }
"=="        { return EQ_OP; }
"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"/"         { return '/'; }
"%"         { return '%'; }
"<"         { return '<'; }
">"         { return '>'; }
"&&"        { return AND_OP; }
"||"        { return OR_OP; }
"!"         { return '!'; }
"{"         { return '{'; }
"}"         { return '}'; }
"("         { return '('; }
")"         { return ')'; }
";"         { return ';'; }
[ \t]+      { /* ignorar */ }
\n          { /* ignorar */ }
\/\/[^\n]*  { /* comentario de linea */ }
\/\*([^*]|\*+[^*/])*\*+\/ { /* comentario multilinea */ }
.           { lexical_errors++; fprintf(stderr, "Error léxico: símbolo '%s' no reconocido en la línea %d\n", yytext, yylineno); }
%%
int yywrap() { return 1; }