CC=gcc
CFLAGS=-I./libdrm/include/drm

OBJS=src/main.o src/util.o src/draw.o libdrm/build/libdrm.a
BIN=drm_test

DIR := ${CURDIR}

$(BIN): libdrm $(OBJS)
	$(CC) -static $(OBJS) -lm -o $(BIN)

libdrm:
	cp libdrm.meson.build libdrm/meson.build
	cd libdrm && meson build
	cd libdrm/build && ninja

clean:
	$(RM) -r $(BIN) $(OBJS)

all: drm_test
