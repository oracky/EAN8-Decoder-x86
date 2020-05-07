CC=gcc
ASMBIN=nasm

all : asm cc link clean
asm : 
	$(ASMBIN) -o ean8.o -f elf -g -l ean8.lst ean8.asm
cc :
	$(CC) -m32 -fpack-struct -c -g -O0 ean8_main.c
link :
	$(CC) -m32 -fpack-struct -g -o ean8_decoder ean8_main.o ean8.o
clean :
	rm *.o
	rm ean8.lst

