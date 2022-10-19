
CC = gcc
CFLAGS = -Wall -g

all: mmu.o test

test: test.c mmu.o
	$(CC) $(CFLAGS) $^ -o $@

mmu.o: mmu.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm *.o test
