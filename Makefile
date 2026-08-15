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
         system/allocate.c \
         system/ext2.c \
         system/syscalls.c \
         system/virtio_pci.c \
         system/virtio_gpu.c \
         system/virtio_gpu_proto.c \
         system/display_manager.c \
         system/virtio_cursor_transport.c

CXX_SRCS = system/mouse.cpp \
	 system/virtio_gpu_cmd.cpp
ASM_SRCS = system/interrupts.asm

C_OBJS = $(addprefix build/, $(notdir $(C_SRCS:.c=.o)))
ASM_OBJS = $(addprefix build/, $(notdir $(ASM_SRCS:.asm=.o)))
CXX_OBJS = $(addprefix build/, $(notdir $(CXX_SRCS:.cpp=_cpp.o)))

OBJS = $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS)

all: clean build run

build: | build_dir $(OBJS)
	# 1. Сборка EFI файла
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o BOOTX64.EFI $(OBJS)
	
	# 2. Создание структуры папок для ISO
	mkdir -p iso_root/EFI/BOOT
	cp BOOTX64.EFI iso_root/EFI/BOOT/BOOTX64.EFI
	echo "FS0:\\EFI\\BOOT\\BOOTX64.EFI" > iso_root/startup.nsh
	
	# 3. Создание большого FAT32 образа с помощью mtools (64 MB)
	dd if=/dev/zero of=iso_root/efiboot.img bs=1M count=64
	mformat -i iso_root/efiboot.img -F -F -c 1 -v "EFI_BOOT" ::
	mmd -i iso_root/efiboot.img ::/EFI
	mmd -i iso_root/efiboot.img ::/EFI/BOOT
	mcopy -i iso_root/efiboot.img BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	
	# 4. Создание финального ISO образа
	xorriso -as mkisofs -R -f -e efiboot.img -no-emul-boot -o analos.iso iso_root

# Явное правило для загрузчика (так как он лежит в папке boot)
build/bootloader.o: boot/bootloader.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

# Обобщенное правило для всех остальных файлов ядра (так как они лежат в папке system)
build/%.o: system/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

build/%_cpp.o: system/%.cpp | build_dir
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: system/%.asm | build_dir
	$(ASM) $(ASMFLAGS) $< -o $@

build_dir:
	mkdir -p build

run:
	qemu-system-x86_64 \
		-machine q35 \
		-bios ./OVMF.fd \
		-m 512M \
		-vga virtio \
		-global virtio-gpu-pci.xres=1024 \
		-global virtio-gpu-pci.yres=768 \
		-display sdl \
		-net none \
		-serial stdio \
		-cdrom analos.iso \
		-drive id=ahcidisk,file=disk.img,if=none,format=raw,cache=writethrough \
		-device ahci,id=ahci \
		-device ide-hd,drive=ahcidisk,bus=ahci.0 \
		-d int \
		-D qemu.log

clean:
	rm -f BOOTX64.EFI analos.iso
	rm -rf iso_root
	rm -rf build

.PHONY: all build run clean build_dir
