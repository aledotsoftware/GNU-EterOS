CC = gcc
LD = ld
CFLAGS = -m64 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os
LDFLAGS = -T linker.ld -nostdlib -m elf_x86_64

all: test.elf

test.o: test.c
	$(CC) $(CFLAGS) -c test.c -o test.o

test.elf: test.o
	$(LD) $(LDFLAGS) -o test.elf test.o

clean:
	rm -f *.o *.elf
