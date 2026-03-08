#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>

#define O_RDWR  0x0002
#define O_CREAT 0x0040

#define FBIOGET_VSCREENINFO 0x4600

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
} fb_info_t;

int main(int argc, char* argv[]) {
    printf("[Eterland] Starting Eterland Compositor...\n");

    /* 1. Open Framebuffer */
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("[Eterland] Error opening /dev/fb0! errno: %d\n", errno);
        return 1;
    }

    /* 2. Get Framebuffer Info */
    fb_info_t fb_info;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_info) != 0) {
        printf("[Eterland] Error getting fb info!\n");
        close(fb_fd);
        return 1;
    }
    printf("[Eterland] FB Info: %dx%d, %d bpp, %d pitch\n", 
           fb_info.width, fb_info.height, fb_info.bpp, fb_info.pitch);

    /* 3. Mmap Framebuffer */
    size_t fb_size = fb_info.height * fb_info.pitch;
    void* fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        printf("[Eterland] Error mapping FB! errno: %d\n", errno);
        close(fb_fd);
        return 1;
    }
    printf("[Eterland] FB mapped at %p\n", fb_ptr);

    /* Paint screen dark blue */
    memset(fb_ptr, 0x11, fb_size); 

    /* 4. Setup IPC Pipe */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        printf("[Eterland] Error creating pipe!\n");
        return 1;
    }

    /* 5. Setup Window SHM */
    const char* shm_name = "eterland_win1";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        printf("[Eterland] Error creating SHM!\n");
        return 1;
    }

    /* Window size: 400x300, 32-bit (ARGB) */
    uint32_t win_width = 400;
    uint32_t win_height = 300;
    size_t win_size = win_width * win_height * 4;
    ftruncate(shm_fd, win_size);

    void* win_ptr = mmap(NULL, win_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (win_ptr == MAP_FAILED) {
        printf("[Eterland] Error mapping SHM window!\n");
        return 1;
    }

    /* 6. Spawn Client */
    int pid = fork();
    if (pid < 0) {
        printf("[Eterland] Fork failed!\n");
        return 1;
    } else if (pid == 0) {
        /* Client Process */
        close(pipefd[0]); /* Close read end */

        printf("[Client] Drawing to SHM buffer...\n");
        uint32_t* pixels = (uint32_t*)win_ptr;
        for (uint32_t y = 0; y < win_height; y++) {
            for (uint32_t x = 0; x < win_width; x++) {
                /* Draw a gradient */
                uint8_t r = (x * 255) / win_width;
                uint8_t g = (y * 255) / win_height;
                uint8_t b = 128;
                pixels[y * win_width + x] = (r << 16) | (g << 8) | b;
            }
        }
        
        /* Send Frame Ready message */
        char msg = 'R'; /* Ready */
        write(pipefd[1], &msg, 1);
        printf("[Client] Frame ready sent to server.\n");

        close(pipefd[1]);
        exit(0);
    }

    /* Server Process */
    close(pipefd[1]); /* Close write end */

    /* 7. Wait for client frame */
    char msg = 0;
    read(pipefd[0], &msg, 1);
    
    if (msg == 'R') {
        printf("[Eterland] Received Frame Ready from client. Compositing...\n");

        /* Copy from SHM to FB (Zero-Copy for the client, the server copies to video RAM) */
        uint32_t* dst = (uint32_t*)fb_ptr;
        uint32_t* src = (uint32_t*)win_ptr;
        
        /* Place window at offset 100, 100 */
        uint32_t offset_x = 100;
        uint32_t offset_y = 100;

        for (uint32_t y = 0; y < win_height; y++) {
            memcpy(&dst[(offset_y + y) * (fb_info.pitch / 4) + offset_x],
                   &src[y * win_width],
                   win_width * 4);
        }
        printf("[Eterland] Composition complete.\n");
    }

    waitpid(pid, NULL, 0);
    shm_unlink(shm_name);
    return 0;
}
