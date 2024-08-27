# Simple makefile to build shared library

libparlay.so: parlay.c parlay.h parlay-internal.h Makefile
	gcc parlay.c -O2 -fPIC -shared -I/usr/include/freetype2 -o libparlay.so

clean:
	rm -rf libparlay.so

.PHONY: clean
