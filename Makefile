all: rl_open

rl_open: src/rl_open.c	src/utils.c
	gcc -I include -o rl_open src/rl_open.c src/utils.c -pthread -lrt


clean:
	rm -rf rl_open  *.o