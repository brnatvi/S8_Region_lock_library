all: rl_open rl_dup


rl_open: src/rl_open.c	src/utils.c src/rl_init_library.c
	gcc  -I include -Wall -g -o rl_open src/rl_open.c src/utils.c src/rl_init_library.c -pthread -lrt

rl_dup: src/rl_dup.c src/utils.c src/rl_init_library.c
	gcc  -I include -Wall -g -o rl_dup src/rl_dup.c src/utils.c src/rl_init_library.c -pthread -lrt



clean:
	rm -rf rl_open rl_dup *.o