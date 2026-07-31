CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
ASM = nasm

CFLAGS = -ffreestanding -nostdlib -mno-red-zone -fno-builtin -Wall -O2 \
         -I. \
         -Isystem

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

ASMFLAGS = -f elf64

LDFLAGS = -Wl,--subsystem,10 \
          -Wl,-entry,efi_main \
          -Wl,--dll \
          -s

C_SRCS = boot/bootloader.c \
         system/kernel.c \
         system/screen.c \
         system/idt.c \
         system/keyboard.c \
         system/mouse.c \
	 system/lib.c \
	 system/ahci.c \
	 system/print.c \
	 system/allocate.c

CXX_SRCS = system/mouse.cpp

ASM_SRCS = system/interrupts.asm



C_OBJS = $(addprefix build/, $(notdir $(C_SRCS:.c=.o)))
ASM_OBJS = $(addprefix build/, $(notdir $(ASM_SRCS:.asm=.o)))
CXX_OBJS = $(addprefix build/, $(notdir $(CXX_SRCS:.cpp=_cpp.o)))

OBJS = $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS)

all: build

build: | build_dir $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o BOOTX64.EFI $(OBJS)
	mkdir -p image/EFI/BOOT
	cp BOOTX64.EFI image/EFI/BOOT/BOOTX64.EFI
	echo "FS0:\\EFI\\BOOT\\BOOTX64.EFI" > image/startup.nsh


build/%.o: boot/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: system/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: system/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@
	
build/lib.o: system/lib.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@ 

build/%_cpp.o: system/%.cpp | build_dir
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: system/%.asm | build_dir
	$(ASM) $(ASMFLAGS) $< -o $@

build_dir:
	mkdir -p build

run:
	$(MAKE) clean
	$(MAKE) build
	qemu-system-x86_64 \
		-bios ./OVMF.fd \
		-net none \
		-m 512M \
		-vga std \
		-global VGA.xres=1024 \
		-global VGA.yres=768 \
		-display gtk \
		-serial stdio \
		-drive if=none,id=usbstick,format=raw,file=fat:rw:image \
		-device usb-ehci,id=ehci \
		-device usb-storage,bus=ehci.0,drive=usbstick \
		-drive id=ahcidisk,file=disk.img,if=none,format=raw \
		-device ahci,id=ahci \
		-device ide-hd,drive=ahcidisk,bus=ahci.0 \
		-machine pc \
		-device isa-applesmc,osk="insertoskhereuphere" \
		-d int \
		-D qemu.log




clean:
	rm -f BOOTX64.EFI
	rm -rf image
	rm -rf build

.PHONY: all build run clean build_dir
