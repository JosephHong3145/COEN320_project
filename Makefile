all:
	gcc ${wildcard *.c} -o out -lrt -lpthread -Wall -g
	./out
