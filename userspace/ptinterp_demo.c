#include <stdio.h>

__attribute__((section(".interp")))
const char pt_interp_path[] = "/ld-eter.elf";

int main(void) {
    printf("[ptinterp-demo] program main reached\n");
    return 0;
}
