all: lib

lib: lib32/hax11.so lib64/hax11.so

lib32:
	mkdir lib32
lib64:
	mkdir lib64

lib32/hax11.so: lib32 lib.c Makefile
	gcc -m32 -Wall -Wextra -g lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE
lib64/hax11.so: lib64 lib.c Makefile
	gcc -m64 -Wall -Wextra -g lib.c -o $@ -fPIC -shared -ldl -D_GNU_SOURCE

install:
	install -m 644 lib32/hax11.so /usr/local/lib32/
	install -m 644 lib64/hax11.so /usr/local/lib64/
	install profile.d/hax11.sh /etc/profile.d/

uninstall:
	rm -f /usr/local/lib32/hax11.so
	rm -f /usr/local/lib64/hax11.so
	rm -f /etc/profile.d/hax11.sh

.PHONY: all lib install
