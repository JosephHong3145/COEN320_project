all:
	gcc ${wildcard *.c} -o out
	./out
