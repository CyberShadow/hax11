PREFIX=/usr/local
LIB32=lib32
LIB64=lib64

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
	install -d $(PREFIX)/$(LIB32)/
	install -d $(PREFIX)/$(LIB64)/
	install -m 644 lib32/hax11.so $(PREFIX)/$(LIB32)/
	install -m 644 lib64/hax11.so $(PREFIX)/$(LIB64)/
	echo "export LD_PRELOAD=$(PREFIX)/\\\$$LIB/hax11.so " > /etc/profile.d/hax11.sh
	chmod 755 /etc/profile.d/hax11.sh

uninstall:
	rm -f $(PREFIX)/$(LIB32)/hax11.so
	rm -f $(PREFIX)/$(LIB64)/hax11.so
	rm -f /etc/profile.d/hax11.sh

.PHONY: all lib install
