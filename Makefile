CC=gcc
CFLAGS=-I/usr/include/libdrm 

OBJS=src/main.o src/util.o libdrm/build/libdrm.a
BIN=drm_test

DIR := ${CURDIR}

INITRAMFS=build/initramfs.cpio.gz
BZIMAGE=build/linux/arch/x86_64/boot/bzImage

kernel:
	mkdir -p build/linux
	make -C linux O=$(DIR)/build/linux allnoconfig
	cp linux.config build/linux/.config
	make -C build/linux -j8

libdrm:
	cp libdrm.meson.build libdrm/meson.build
	cd libdrm && meson build
	cd libdrm/build && ninja

initramfs:
	rm -rf initramfs
	mkdir -p initramfs/{bin,sbin,etc,proc,sys,newroot}
	wget https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-x86_64 -O initramfs/bin/busybox
	chmod +x initramfs/bin/busybox

	ln -s busybox initramfs/bin/sh
	ln -s busybox initramfs/bin/ls

	cp $(BIN) initramfs/bin
	cd initramfs && find -print0 | cpio -0oH newc | gzip -9 > ../$(INITRAMFS)

$(BIN): $(OBJS)
	$(CC) -static $(OBJS) -o $(BIN)

qemu:
	qemu-system-x86_64 \
	    -kernel $(BZIMAGE) \
	    -initrd $(INITRAMFS) \
	    -append "init=/bin/sh"

clean:
	$(RM) -r build initramfs $(BIN) $(OBJS)

all: kernel init initramfs
