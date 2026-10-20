compilador: lex.yy.c parser.tab.c ast.c symbol_table.c intermediate_code.c main.c
	gcc -o compilador lex.yy.c parser.tab.c ast.c symbol_table.c intermediate_code.c main.c -lfl

lex.yy.c: lexer.l
	flex lexer.l

parser.tab.c: parser.y
	bison -d parser.y

test: compilador
	./run_tests.sh

clean:
	rm -f compilador lex.yy.c parser.tab.c parser.tab.h

.PHONY: test clean
