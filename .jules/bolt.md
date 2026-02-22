## 2026-02-14 - [Cached System Stats]
**Learning:** Calculating memory statistics involves 64-bit division `(total - free) / (1024*1024)`. Doing this every frame (60Hz) is unnecessary for values that change slowly.
**Action:** Cache UI indicators that require expensive math (div/mod) and update them at a lower frequency (e.g., 2Hz or 1Hz) to spare CPU cycles for the render loop.

## 2026-10-14 - [Word-at-a-time String Comparison]
**Learning:** When optimizing `memcmp` with word-sized (e.g., `uint64_t`) comparisons on Little Endian architectures (x86_64), the integer comparison result (`a < b` or `a > b`) does NOT necessarily match the lexicographical byte-wise comparison result due to byte significance order.
**Action:** Use word-sized comparisons ONLY to check for equality (`a == b`). Upon finding the first differing word, fall back to a byte-wise comparison loop to determine the correct return value sign.

## 2026-10-24 - [Fixed Point Gradient & Unsigned Underflow]
**Learning:** Calculating gradients using `(end - start) * i / h` with `uint32_t` components causes underflow when `end < start`, resulting in corrupted colors. Also, division inside the loop is expensive.
**Action:** Use `int32_t` fixed-point arithmetic (16.16) to handle negative slopes correctly and replace per-pixel division with integer addition.

## 2026-11-23 - [Glow Overdraw & VRAM Reads]
**Learning:** Alpha blending operations like `omni_fill_rect_alpha` require reading from the framebuffer (VRAM), which is extremely slow due to uncached memory access. Drawing a full filled rectangle for a border/glow effect and then overwriting the center wastes ~90% of bandwidth.
**Action:** Use a "hollow" drawing function (4 strips) for borders and glows to eliminate overdraw of the central area, especially for large UI elements.

## 2026-12-05 - [Naive Userspace String Functions]
**Learning:** Userspace string functions (libc) often lag behind kernel implementations in performance optimizations. A naive `while(*p) p++` `strlen` was ~5.3x slower than the kernel's SWAR implementation for large strings.
**Action:** Audit userspace libraries for naive implementations of critical functions and port existing kernel optimizations (like SWAR or SIMD) where appropriate.
