gcc main.c -o main.o -c -w \
    -fno-stack-protector \
    -fpic -fshort-wchar -mno-red-zone \
    -I/usr/include/efi \
    -I/usr/include/efi/x86_64

ld -shared -Bsymbolic \
    -L/usr/lib \
    -T/usr/lib/elf_x86_64_efi.lds \
    /usr/lib/crt0-efi-x86_64.o main.o \
    -o main.so \
    -lgnuefi -lefi

objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j ".rel.*" -j ".rela.*" -j .reloc --target efi-app-x86_64 --subsystem=10 main.so main.efi
