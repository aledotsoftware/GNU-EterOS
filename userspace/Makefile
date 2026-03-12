CC = gcc
LD = ld
CFLAGS = -m64 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -Ilibc/include
LDFLAGS = -T linker.ld -nostdlib -m elf_x86_64

LIBC_SRC = libc/src/start.c libc/src/syscall.c libc/src/stdio.c libc/src/string.c libc/src/malloc.c libc/src/errno.c libc/src/network.c libc/src/posix.c libc/src/stdlib.c libc/src/signal.c libc/src/sha256.c libc/src/strtok.c libc/src/pthread.c libc/src/dirent.c libc/src/time.c
LIBC_OBJ = $(LIBC_SRC:.c=.o)

all: test.elf test_net.elf exec_test.elf test_libc_advanced.elf test_signal.elf test_futex_security.elf test_stdio.elf test_malloc.elf test_pthread.elf login.elf passwd.elf useradd.elf userdel.elf sh.elf test_security.elf test_shm.elf marea_shell.elf eterland.elf

libc/src/%.o: libc/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test.o: test.c
	$(CC) $(CFLAGS) -c test.c -o test.o

exec_test.o: exec_test.c
	$(CC) $(CFLAGS) -c exec_test.c -o exec_test.o

test_libc_advanced.o: test_libc_advanced.c
	$(CC) $(CFLAGS) -c test_libc_advanced.c -o test_libc_advanced.o

test.elf: $(LIBC_OBJ) test.o
	$(LD) $(LDFLAGS) -o test.elf $(LIBC_OBJ) test.o

test_net.o: test_net.c
	$(CC) $(CFLAGS) -c test_net.c -o test_net.o

test_net.elf: $(LIBC_OBJ) test_net.o
	$(LD) $(LDFLAGS) -o test_net.elf $(LIBC_OBJ) test_net.o

exec_test.elf: $(LIBC_OBJ) exec_test.o
	$(LD) $(LDFLAGS) -o exec_test.elf $(LIBC_OBJ) exec_test.o

test_libc_advanced.elf: $(LIBC_OBJ) test_libc_advanced.o
	$(LD) $(LDFLAGS) -o test_libc_advanced.elf $(LIBC_OBJ) test_libc_advanced.o

test_signal.o: test_signal.c
	$(CC) $(CFLAGS) -c test_signal.c -o test_signal.o

test_signal.elf: $(LIBC_OBJ) test_signal.o
	$(LD) $(LDFLAGS) -o test_signal.elf $(LIBC_OBJ) test_signal.o

test_futex_security.o: test_futex_security.c
	$(CC) $(CFLAGS) -c test_futex_security.c -o test_futex_security.o

test_futex_security.elf: $(LIBC_OBJ) test_futex_security.o
	$(LD) $(LDFLAGS) -o test_futex_security.elf $(LIBC_OBJ) test_futex_security.o

test_stdio.o: test_stdio.c
	$(CC) $(CFLAGS) -c test_stdio.c -o test_stdio.o

test_stdio.elf: $(LIBC_OBJ) test_stdio.o
	$(LD) $(LDFLAGS) -o test_stdio.elf $(LIBC_OBJ) test_stdio.o

test_malloc.o: test_malloc.c
	$(CC) $(CFLAGS) -c test_malloc.c -o test_malloc.o

test_malloc.elf: $(LIBC_OBJ) test_malloc.o
	$(LD) $(LDFLAGS) -o test_malloc.elf $(LIBC_OBJ) test_malloc.o

test_pthread.o: test_pthread.c
	$(CC) $(CFLAGS) -c test_pthread.c -o test_pthread.o

test_pthread.elf: $(LIBC_OBJ) test_pthread.o
	$(LD) $(LDFLAGS) -o test_pthread.elf $(LIBC_OBJ) test_pthread.o

login.o: login.c
	$(CC) $(CFLAGS) -c login.c -o login.o

login.elf: $(LIBC_OBJ) login.o
	$(LD) $(LDFLAGS) -o login.elf $(LIBC_OBJ) login.o

passwd.o: passwd.c
	$(CC) $(CFLAGS) -c passwd.c -o passwd.o

passwd.elf: $(LIBC_OBJ) passwd.o
	$(LD) $(LDFLAGS) -o passwd.elf $(LIBC_OBJ) passwd.o

useradd.o: useradd.c
	$(CC) $(CFLAGS) -c useradd.c -o useradd.o

useradd.elf: $(LIBC_OBJ) useradd.o
	$(LD) $(LDFLAGS) -o useradd.elf $(LIBC_OBJ) useradd.o

userdel.o: userdel.c
	$(CC) $(CFLAGS) -c userdel.c -o userdel.o

userdel.elf: $(LIBC_OBJ) userdel.o
	$(LD) $(LDFLAGS) -o userdel.elf $(LIBC_OBJ) userdel.o

test_security.o: test_security.c
	$(CC) $(CFLAGS) -c test_security.c -o test_security.o

test_security.elf: $(LIBC_OBJ) test_security.o
	$(LD) $(LDFLAGS) -o test_security.elf $(LIBC_OBJ) test_security.o

test_shm.o: test_shm.c
	$(CC) $(CFLAGS) -c test_shm.c -o test_shm.o

test_shm.elf: $(LIBC_OBJ) test_shm.o
	$(LD) $(LDFLAGS) -o test_shm.elf $(LIBC_OBJ) test_shm.o

sh.o: sh.c
	$(CC) $(CFLAGS) -c sh.c -o sh.o

sh.elf: $(LIBC_OBJ) sh.o
	$(LD) $(LDFLAGS) -o sh.elf $(LIBC_OBJ) sh.o

clean:
	rm -f *.o *.elf libc/src/*.o

test_net_echo.o: test_net_echo.c
	$(CC) $(CFLAGS) -c test_net_echo.c -o test_net_echo.o

test_net_echo.elf: $(LIBC_OBJ) test_net_echo.o
	$(LD) $(LDFLAGS) -o test_net_echo.elf $(LIBC_OBJ) test_net_echo.o

marea_shell.o: marea_shell.c
	$(CC) $(CFLAGS) -c marea_shell.c -o marea_shell.o

marea_shell.elf: $(LIBC_OBJ) marea_shell.o
	$(LD) $(LDFLAGS) -o marea_shell.elf $(LIBC_OBJ) marea_shell.o

eterland.o: eterland.c
	$(CC) $(CFLAGS) -c eterland.c -o eterland.o

eterland.elf: $(LIBC_OBJ) eterland.o
	$(LD) $(LDFLAGS) -o eterland.elf $(LIBC_OBJ) eterland.o
