all:
	qcc ${wildcard *.c} -o out
	./out
