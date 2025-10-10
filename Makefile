# Simple makefile to build shared library

libparlay.so: parlay.c parlay.h parlay-internal.h Makefile
	gcc parlay.c -DPARLAY_USE_MINIXML=0 -O2 -fPIC -I/usr/include/freetype2 -shared -Wl,--as-needed -lfreetype -o libparlay.so

clean:
	rm -rf libparlay.so

.PHONY: clean
