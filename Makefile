all: client lib

client: client.c Makefile
	gcc -std=c99 -lxcb -lxcb-randr -lxcb-xinerama -lxcb-xtest -lxcb-shape -lxcb-util -g -o client client.c

lib: lib32/dell4khack.so lib64/dell4khack.so

lib32/dell4khack.so: lib.c Makefile
	gcc -m32 lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE
lib64/dell4khack.so: lib.c Makefile
	gcc -m64 lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE

install:
	install -m 644 lib32/dell4khack.so /usr/local/lib32/
	install -m 644 lib64/dell4khack.so /usr/local/lib64/
	install profile.d/dell4khack.sh /etc/profile.d/

.PHONY: all lib install
