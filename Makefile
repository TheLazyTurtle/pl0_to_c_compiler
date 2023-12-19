CFLAGS = -Wall -lc -lm
SRC=$(wildcard *.cpp)

all: run

run: build test
	./test ./tests/test.pl0

build: $(SRC)
	gcc -o $@ $^ $(CFLAGS)

