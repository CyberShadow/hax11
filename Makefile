all: lib

lib: lib32/hax11.so lib64/hax11.so

lib32/hax11.so: lib.c Makefile
	gcc -m32 -Wall -Wextra -g lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE
lib64/hax11.so: lib.c Makefile
	gcc -m64 -Wall -Wextra -g lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE

install:
	install -m 644 lib32/hax11.so /usr/local/lib32/
	install -m 644 lib64/hax11.so /usr/local/lib64/
	install profile.d/hax11.sh /etc/profile.d/

.PHONY: all lib install
