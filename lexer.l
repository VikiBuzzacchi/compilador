%{
    // Analizador Léxico

    #include "parser.tab.h"
    #include <string.h>
%}

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
[ \t\n]+    { /* ignorar */ }
.           { printf("Error léxico: %s\n", yytext); }
%%
int yywrap() { return 1; }