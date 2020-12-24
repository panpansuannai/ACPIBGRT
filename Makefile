ARCH = x86_64
CC = gcc
CFLAGS = -c -w -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
	 -I/usr/include/efi -I/usr/include/efi/$(ARCH)
LDS = /usr/lib/elf_$(ARCH)_efi.lds
LDFLAGS = -shared -Bsymbolic -L/usr/lib -T $(LDS) -lgnuefi -lefi
LD = ld

all:main.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym  -j .rel -j .rela -j ".rel.*" \
		-j ".rela.*" -j .reloc \
		--target efi-app-x86_64 --subsystem=10 main.so main.efi
main.so:main.o
	$(LD) $(LDFLAGS) /usr/lib/crt0-efi-x86_64.o main.o \
	    -o main.so
main.o:main.c
	$(CC) $(CFLAGS) -o main.o main.c
clean:
	rm main.o main.efi main.so
