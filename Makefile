CC=gcc
CFLAGS=-I/usr/include/libdrm 

OBJS=src/main.o src/util.o libdrm/build/libdrm.a
BIN=drm_test

DIR := ${CURDIR}

INITRAMFS=build/initramfs.cpio.gz
BZIMAGE=build/linux/arch/x86_64/boot/bzImage

libdrm:
	cp libdrm.meson.build libdrm/meson.build
	cd libdrm && meson build
	cd libdrm/build && ninja

$(BIN): $(OBJS)
	$(CC) -static $(OBJS) -o $(BIN)

qemu:
	qemu-system-x86_64 \
	    -kernel $(BZIMAGE) \
	    -initrd $(INITRAMFS) \
	    -append "init=/bin/sh"

clean:
	$(RM) -r build initramfs $(BIN) $(OBJS)

all: drm_test
