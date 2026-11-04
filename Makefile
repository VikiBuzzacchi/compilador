SRCS = lex.yy.c parser.tab.c ast.c symbol_table.c intermediate_code.c object_code.c optimizer.c main.c
HDRS = ast.h symbol_table.h tac.h intermediate_code.h object_code.h optimizer.h parser.tab.h

compilador: $(SRCS) $(HDRS)
	gcc -o compilador $(SRCS) -lfl

c-tds: compilador
	cp compilador c-tds

lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

parser.tab.c: parser.y
	bison -d parser.y

parser.tab.h: parser.tab.c

test: compilador
	./run_tests.sh

clean:
	rm -f compilador lex.yy.c parser.tab.c parser.tab.h *.ass *.out *.ci

.PHONY: test clean
