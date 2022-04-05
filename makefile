includes=-I /usr/local/Cellar/icarus-verilog/10.2_1/include/
cflags=-g -Wall -Wextra -fsanitize=address -fsanitize=undefined

all: bin

sym: main.v makefile
	iverilog -o sym -Wall main.v

bin: main.c makefile
	clang $(cflags) main.c -o bin

#for windows
winbin: main.c makefile
	clang -g -Wall -Wextra main.c -o winbin

runsym: sym 
	./sym

runbin:	bin
	./bin

runwinbin: winbin
	./winbin


cling: 
	cling Tc main.c