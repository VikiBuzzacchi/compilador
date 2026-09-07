%{
    // Analizador Léxico

    #include <stdio.h>
    #include <string.h>
    #include "parser.tab.h"

    int lexical_errors = 0;
%}

%option yylineno

%%
"main"      { return MAIN; }
"int"       { return INT; }
"bool"      { return BOOL; }
"true"      { return TRUE; }
"false"     { return FALSE; }
"return"    { return RETURN; }
"void"      { return VOID; }
[a-zA-Z][a-zA-Z0-9]* { yylval.id = strdup(yytext); return ID; }
[0-9]+      { yylval.val = atoi(yytext); return CONST; }
"="         { return '='; }
"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"{"         { return '{'; }
"}"         { return '}'; }
"("         { return '('; }
")"         { return ')'; }
";"         { return ';'; }
[ \t]+      { /* ignorar */ }
\n          { /* ignorar */ }
.           { lexical_errors++; fprintf(stderr, "Error léxico: símbolo '%s' no reconocido en la línea %d\n", yytext, yylineno); }
%%
int yywrap() { return 1; }