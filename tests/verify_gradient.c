#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

// Function to test
void test_gradient_logic(uint32_t color_top, uint32_t color_bottom, int h) {
    if (h <= 0) return;

    printf("Testing gradient: 0x%06X -> 0x%06X (h=%d)... ", color_top, color_bottom, h);

    // New logic: Fixed Point 16.16
    uint32_t tr = (color_top >> 16) & 0xFF;
    uint32_t tg = (color_top >> 8) & 0xFF;
    uint32_t tb = color_top & 0xFF;
    uint32_t br = (color_bottom >> 16) & 0xFF;
    uint32_t bg = (color_bottom >> 8) & 0xFF;
    uint32_t bb = color_bottom & 0xFF;

    int32_t r_val = (int32_t)tr << 16;
    int32_t g_val = (int32_t)tg << 16;
    int32_t b_val = (int32_t)tb << 16;

    int32_t r_step = (((int32_t)br - (int32_t)tr) << 16) / h;
    int32_t g_step = (((int32_t)bg - (int32_t)tg) << 16) / h;
    int32_t b_step = (((int32_t)bb - (int32_t)tb) << 16) / h;

    for (int i = 0; i < h; i++) {
        uint32_t r = (uint32_t)(r_val >> 16);
        uint32_t g = (uint32_t)(g_val >> 16);
        uint32_t b = (uint32_t)(b_val >> 16);

        // Float verification
        // Using strict integer division simulation as per original C semantics?
        // Original: tr + ((br - tr) * i) / h
        // But original was buggy for negative. So we compare against float interpolation.

        float r_expected = tr + ((float)br - tr) * i / h;
        float g_expected = tg + ((float)bg - tg) * i / h;
        float b_expected = tb + ((float)bb - tb) * i / h;

        // Allow +/- 1 due to integer truncation vs float rounding
        if (abs((int)r - (int)r_expected) > 1) {
            printf("FAIL R at i=%d: Got %u, Expected %.2f\n", i, r, r_expected);
            exit(1);
        }
        if (abs((int)g - (int)g_expected) > 1) {
            printf("FAIL G at i=%d: Got %u, Expected %.2f\n", i, g, g_expected);
            exit(1);
        }
        if (abs((int)b - (int)b_expected) > 1) {
            printf("FAIL B at i=%d: Got %u, Expected %.2f\n", i, b, b_expected);
            exit(1);
        }

        r_val += r_step;
        g_val += g_step;
        b_val += b_step;
    }
    printf("PASS\n");
}

int main() {
    // Test 1: Increasing
    test_gradient_logic(0x000000, 0xFFFFFF, 100);

    // Test 2: Decreasing
    test_gradient_logic(0xFFFFFF, 0x000000, 100);

    // Test 3: Mixed (R up, G down, B same)
    test_gradient_logic(0x00FF00, 0xFF0000, 50);

    // Test 4: Small height
    test_gradient_logic(0x102030, 0x405060, 5);

    return 0;
}
