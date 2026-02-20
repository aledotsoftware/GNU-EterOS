CC = gcc
LD = ld
CFLAGS = -m64 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -Ilibc/include
LDFLAGS = -T linker.ld -nostdlib -m elf_x86_64

LIBC_SRC = libc/src/start.c libc/src/syscall.c libc/src/stdio.c libc/src/string.c libc/src/malloc.c libc/src/errno.c libc/src/network.c
LIBC_OBJ = $(LIBC_SRC:.c=.o)

all: test.elf test_net.elf

libc/src/%.o: libc/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test.o: test.c
	$(CC) $(CFLAGS) -c test.c -o test.o

test.elf: $(LIBC_OBJ) test.o
	$(LD) $(LDFLAGS) -o test.elf $(LIBC_OBJ) test.o

test_net.o: test_net.c
	$(CC) $(CFLAGS) -c test_net.c -o test_net.o

test_net.elf: $(LIBC_OBJ) test_net.o
	$(LD) $(LDFLAGS) -o test_net.elf $(LIBC_OBJ) test_net.o

clean:
	rm -f *.o *.elf libc/src/*.o
