# minios build. Run inside WSL2 Ubuntu (see INSTALL.md).
#
#   make toolchain      zig (userspace) + Ubuntu 14.04 chroot with gcc 4.7 (optional kernel)
#   make host test      PC build of the UI + unit test
#   make minios         static ARM binary
#   make release BACKUP=path/kindle-full.img
#                       dist/minios-release.zip; the backup provides Amazon's kernel
#                       modules and the restore images (INSTALL.md)
#   make kernel         optional: our own build of Amazon's kernel (not installed)

ZIG_VER    = 0.13.0
MUPDF_VER  = 1.23.11
ZIG_URL    = https://ziglang.org/download/$(ZIG_VER)/zig-linux-x86_64-$(ZIG_VER).tar.xz
MUPDF_URL  = https://mupdf.com/downloads/archive/mupdf-$(MUPDF_VER)-source.tar.gz
BUSYBOX_URL = https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv7l

# full eMMC image taken from the stock Kindle (INSTALL.md step 4)
BACKUP ?= backup/kindle-full.img
BACKUP_DIR = $(dir $(BACKUP))
# Amazon "Kindle Source Code Notice" tarball for Kindle 5th gen, firmware 4.1.4
# (only for `make kernel`):
# https://s3.amazonaws.com/kindledownloads/Kindle_src_4.1.4_3543630001.tar.gz
KINDLE_SRC ?= dl/kindle-src.tar.gz
KERNEL_DEFCONFIG ?= imx50_tequila_defconfig

TOP     := $(CURDIR)
ZIG     = $(TOP)/tools/zig/zig
TARGET  = arm-linux-musleabihf -mcpu=cortex_a8
TRUSTY  = tools/trusty
KGCC    = $(TRUSTY)/usr/bin/arm-linux-gnueabi-gcc
KBUILD  = sudo sh -c 'mkdir -p $(TRUSTY)/minos && { mountpoint -q $(TRUSTY)/minos || mount --bind $(TOP) $(TRUSTY)/minos; } && chroot $(TRUSTY) /bin/sh /minos/scripts/kbuild.sh $$0 $$1'
CFLAGS  = -std=gnu11 -Os -Wall -Wextra -Wno-unused-parameter -Ithird_party/mupdf/include
SRC     = src/main.c src/gfx.c src/library.c src/reader.c src/pos.c \
          src/games.c src/sudoku.c src/checkers.c src/chess.c
DEV_SRC = src/eink.c src/input.c src/usb.c
MUPDF_FLAGS = build=release shared=no HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no \
              HAVE_TESSERACT=no HAVE_LEPTONICA=no XCFLAGS="-DTOFU_CJK"
MUPDF_ARM  = build/mupdf-arm/libmupdf.a build/mupdf-arm/libmupdf-third.a
MUPDF_HOST = build/mupdf-host/libmupdf.a build/mupdf-host/libmupdf-third.a

.PHONY: all toolchain host test minios kernel rootfs release restore-images stock-modules list-configs clean

all: minios

# ---- toolchains and third-party sources -------------------------------------
dl:
	mkdir -p dl

toolchain: tools/zig/zig
	sudo apt-get install -y build-essential genext2fs e2fsprogs zip

tools/zig/zig: | dl
	curl -fL -o dl/zig.tar.xz $(ZIG_URL)
	mkdir -p tools/zig && tar xJf dl/zig.tar.xz -C tools/zig --strip-components=1

# Lab126's 2.6.31 kernel only builds with a toolchain of its era.
$(KGCC):
	sudo apt-get install -y debootstrap
	sudo debootstrap --variant=buildd --components=main,universe \
		--include=gcc-4.7-arm-linux-gnueabi,u-boot-tools,bc,lzop,perl,libncurses5-dev \
		trusty $(TRUSTY) http://archive.ubuntu.com/ubuntu
	sudo ln -sf arm-linux-gnueabi-gcc-4.7 $(KGCC)
	sudo ln -sf arm-linux-gnueabi-cpp-4.7 $(TRUSTY)/usr/bin/arm-linux-gnueabi-cpp

third_party/mupdf/Makefile: | dl
	curl -fL -o dl/mupdf.tar.gz $(MUPDF_URL)
	mkdir -p third_party/mupdf && tar xzf dl/mupdf.tar.gz -C third_party/mupdf --strip-components=1

third_party/busybox: | dl
	mkdir -p third_party && curl -fL -o $@ $(BUSYBOX_URL) && chmod +x $@

# ---- minios ------------------------------------------------------------------
$(MUPDF_ARM): third_party/mupdf/Makefile tools/zig/zig
	$(MAKE) -C third_party/mupdf $(MUPDF_FLAGS) OS=Linux CROSSCOMPILE=yes HAVE_OBJCOPY=no \
		CC="$(ZIG) cc -target $(TARGET)" CXX="$(ZIG) c++ -target $(TARGET)" \
		AR="$(ZIG) ar" OUT=$(TOP)/build/mupdf-arm libs

$(MUPDF_HOST): third_party/mupdf/Makefile
	$(MAKE) -C third_party/mupdf $(MUPDF_FLAGS) OUT=$(TOP)/build/mupdf-host libs

minios: build/minios
build/minios: $(SRC) $(DEV_SRC) src/minios.h src/games.h config.h $(MUPDF_ARM)
	$(ZIG) cc -target $(TARGET) -static $(CFLAGS) $(SRC) $(DEV_SRC) $(MUPDF_ARM) -lc++ -lm -o $@

host: build/minios-host
build/minios-host: $(SRC) src/host.c src/minios.h src/games.h config.h $(MUPDF_HOST)
	$(CC) -DHOST $(CFLAGS) $(SRC) src/host.c $(MUPDF_HOST) -lstdc++ -lm -o $@

test: build/pos_test build/games_test
	cd build && ./pos_test && ./games_test
build/games_test: src/sudoku.c src/checkers.c src/chess.c tests/games_test.c src/games.h config.h
	mkdir -p build && $(CC) -O2 -Wall -Wextra $(filter %.c,$^) -o $@
build/pos_test: src/pos.c tests/pos_test.c
	mkdir -p build && $(CC) -Wall -Wextra $^ -o $@

# ---- Amazon's pieces, cut from the backup ------------------------------------
# eMMC layout of this unit (NOTES.md, scripts/layout.env): p1 = system,
# p2 = diags system, diags kernel at byte 14946304. The main kernel at
# byte 266240 is never written by minios.
restore-images: $(BACKUP_DIR)p1.img
$(BACKUP_DIR)p1.img: $(BACKUP)
	dd if=$(BACKUP) of=$(BACKUP_DIR)p1.img bs=32768 skip=1024 count=11200
	dd if=$(BACKUP) of=$(BACKUP_DIR)p2.img bs=32768 skip=12224 count=2048
	dd if=$(BACKUP) of=$(BACKUP_DIR)diags-kernel.img bs=512 skip=29192 count=10005
	cd $(BACKUP_DIR) && sha256sum p1.img p2.img diags-kernel.img > SHA256SUMS

stock-modules: build/stock-modules
build/stock-modules: $(BACKUP_DIR)p1.img
	rm -rf $@ && mkdir -p $@
	debugfs -R "rdump /lib/modules $@" $(BACKUP_DIR)p1.img

# ---- optional: our own kernel from Amazon's GPL source ------------------------
build/src/.unpacked:
	@test -f $(KINDLE_SRC) || { echo "Download the Kindle 4.1.4 GPL source to $(KINDLE_SRC)"; exit 1; }
	mkdir -p build/src && tar xf $(KINDLE_SRC) -C build/src
	cd build/src && for t in $$(find . -name 'linux-*.tar.*'); do tar xf $$t; done
	touch $@

list-configs: build/src/.unpacked
	@ls -d build/src/linux-*/arch/arm/configs/* | grep -i -e imx5 -e mx50 -e yoshi -e tequila || true

kernel: build/uImage
build/uImage: build/src/.unpacked kernel.config $(wildcard kernel/*.h kernel/linux/*.h) scripts/kbuild.sh $(KGCC)
	$(KBUILD) kernel $(KERNEL_DEFCONFIG)

# ---- rootfs and release ----------------------------------------------------------
rootfs: build/rootfs.img
build/rootfs.img: build/minios build/stock-modules third_party/busybox scripts/mkrootfs.sh $(wildcard rootfs/etc/*)
	sh scripts/mkrootfs.sh build/rootfs.img

# dist/minios/ is copied as-is to the Kindle's USB drive as minios-flash/
release: build/rootfs.img third_party/busybox
	rm -rf dist/minios && mkdir -p dist/minios
	cp build/rootfs.img build/minios third_party/busybox dist/minios/
	cp scripts/flash.sh scripts/discover.sh scripts/layout.env INSTALL.md NOTES.md dist/minios/
	cd dist/minios && sha256sum rootfs.img minios busybox > SHA256SUMS
	cd dist && rm -f minios-release.zip && zip -r minios-release.zip minios

clean:
	rm -rf build dist
