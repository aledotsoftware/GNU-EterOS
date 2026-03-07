#include <stdio.h>

int main() {
    int x_start = -500;
    int y_start = -300;
    int width = 1024;
    int height = 768;
    int fb_w = 1920;
    int fb_h = 1080;

    int x_end = x_start + width;
    int y_end = y_start + height;

    if (x_start >= fb_w || y_start >= fb_h || x_end <= 0 || y_end <= 0) {
        printf("Clipped fully!\n");
        return 0;
    }

    int draw_x_start = x_start < 0 ? 0 : x_start;
    int draw_y_start = y_start < 0 ? 0 : y_start;
    int draw_x_end = x_end > fb_w ? fb_w : x_end;
    int draw_y_end = y_end > fb_h ? fb_h : y_end;

    int draw_w = draw_x_end - draw_x_start;
    int draw_h = draw_y_end - draw_y_start;

    printf("Original: (%d, %d) to (%d, %d)\n", x_start, y_start, x_end, y_end);
    printf("Clipped: (%d, %d) to (%d, %d)\n", draw_x_start, draw_y_start, draw_x_end, draw_y_end);
    printf("Clipped Size: %dx%d\n", draw_w, draw_h);

    return 0;
}
