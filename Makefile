all: client lib.so

client: client.c Makefile
	gcc -std=c99 -lxcb -lxcb-randr -lxcb-xinerama -lxcb-xtest -lxcb-shape -lxcb-util -g -o client client.c

lib.so: lib.c Makefile
	gcc lib.c -o dell4khack.so -fPIC -shared -ldl -D_GNU_SOURCE
