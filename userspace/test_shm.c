#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/wait.h>

/* extern int errno; */

int main(void) {
    printf("Starting SHM test...\n");

    const char* shm_name = "test_shm";
    /* O_CREAT=0x0040, O_RDWR=0x0002 */
    int fd = shm_open(shm_name, 0x0042, 0666);
    if (fd < 0) {
        printf("shm_open failed! errno=%d\n", errno);
        return 1;
    }
    printf("shm_open succeeded, fd=%d\n", fd);

    /* Allocate 1 page */
    long ret = ftruncate(fd, 4096);
    if (ret < 0) {
        printf("ftruncate failed! ret=%ld, errno=%d\n", ret, errno);
        return 1;
    }
    printf("ftruncate succeeded\n");

    /* Map shared */
    void* addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap MAP_SHARED failed! errno=%d\n", errno);
        return 1;
    }
    printf("mmap MAP_SHARED succeeded, addr=%p\n", addr);

    /* Write data */
    char* str = (char*)addr;
    strcpy(str, "Hello from Eterland SHM!");

    int pid = fork();
    if (pid < 0) {
        printf("fork failed!\n");
        return 1;
    } else if (pid == 0) {
        /* Child */
        printf("Child mapping shm again...\n");
        int cfd = shm_open(shm_name, 0x0002, 0666);
        if (cfd < 0) {
            printf("Child shm_open failed!\n");
            exit(1);
        }
        
        void* caddr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, cfd, 0);
        if (caddr == MAP_FAILED) {
            printf("Child mmap failed!\n");
            exit(1);
        }
        
        char* cstr = (char*)caddr;
        printf("Child read from SHM: %s\n", cstr);
        
        if (strcmp(cstr, "Hello from Eterland SHM!") == 0) {
            printf("Child: SHM match success!\n");
            strcpy(cstr, "Child says hi!");
        } else {
            printf("Child: SHM mismatch!\n");
        }
        exit(0);
    } else {
        /* Parent */
        waitpid(pid, NULL, 0);
        
        printf("Parent read after child: %s\n", str);
        if (strcmp(str, "Child says hi!") == 0) {
            printf("Parent: SHM update success!\n");
            printf("TEST PASSED\n");
        } else {
            printf("Parent: SHM update failed!\n");
        }
        
        shm_unlink(shm_name);
    }
    
    return 0;
}
