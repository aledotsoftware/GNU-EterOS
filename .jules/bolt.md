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

## 2026-11-24 - [Direct Framebuffer Access]
**Learning:** Per-pixel function calls (`framebuffer_putpixel`) in tight rendering loops add significant overhead due to repeated bounds checks and address calculations.
**Action:** For high-performance rendering (e.g., compositing), access the framebuffer memory directly with pointer arithmetic, lifting invariant calculations out of the loop, especially for known BPPs (32-bit).

## 2026-11-25 - [Unaligned Access on x86_64]
**Learning:** On modern x86_64 processors, unaligned 64-bit memory access has negligible performance penalty for general purpose operations. Explicit alignment checks in `memcmp` add complexity without significant benefit compared to a simple cast-and-loop approach.
**Action:** When optimizing `memcmp` or similar functions for x86_64, straightforward 64-bit loops (checking for equality) are preferred over complex alignment handling logic, yielding massive speedups (~5x) over byte-wise loops.

## 2026-03-03 - [Framebuffer Contiguous Fast Path]
**Learning:** Drawing operations on framebuffers (like flushing or clearing rects) often span the entire width of the screen. When the width of the rectangle multiplied by the bytes per pixel equals the framebuffer pitch (`w * bytes_per_pixel == fb_pitch`), the rows are completely contiguous in memory.
**Action:** Instead of looping over rows and calling `memcpy` or `memset32` for each row, detect the contiguous case and use a single block operation (`memcpy` or `memset32`) for the entire area (`w * h`). This eliminates loop overhead and maximizes memory bandwidth utilization.
## 2026-03-03 - [Opaque Window Fast Path]
**Learning:** Software compositing loops that perform per-pixel alpha blending (transparency checks) are extremely slow for large opaque regions. A standard 800x600 window requires evaluating nearly half a million pixels individually.
**Action:** Use a property flag (e.g., `WIN_OPAQUE`) to identify fully opaque windows at creation time. This allows the compositor to skip pixel-by-pixel rendering and utilize massive, highly-optimized `memcpy` operations for rendering, reducing drawing time by more than 50%.
## 2026-12-05 - [Framebuffer Bulk Copy]
**Learning:** In operations that flush rectangular regions to the framebuffer (`framebuffer_flush_rect`), copying line-by-line is inefficient if the rectangle width matches the screen width (pitch). Full-screen flushes represent a single contiguous memory block.
**Action:** Add a fast-path to check if the row length equals the framebuffer pitch (`row_len == fb_pitch`). If so, copy the entire block using a single `memcpy` call instead of iterating row by row. This provides measurable speedups for full-screen compositing.
## 2026-12-04 - [x86_64 rep movsq Setup Overhead]
**Learning:** While `rep movsq` combined with `rep movsb` provides excellent block copy performance for large strings on x86_64 (due to hardware fast-string optimizations), it introduces noticeable setup overhead for very small block copies (< 64 bytes). For small allocations, relying strictly on `rep movsq` incurs a performance penalty compared to a simple unrolled assignment loop.
**Action:** Always include a fast-path fallback utilizing unrolled 64-bit assignments for small copy sizes (< 64 bytes) in fundamental memory operations like `memcpy` before falling back to `rep` instructions for bulk operations.
## 2026-11-26 - [Framebuffer Block Copy Optimization]
**Learning:** Flushing rectangles row-by-row in the framebuffer introduces overhead from loop iterations and multiple function calls even with optimized `memcpy`. When a dirty rectangle spans the full width of the framebuffer (`row_len == fb_pitch`), the region is perfectly contiguous in memory.
**Action:** Always check if the drawing area spans the full pitch width, and if so, use a single, highly efficient `memcpy` block operation to copy the entire contiguous memory region at once.

## 2026-12-05 - [Framebuffer Image Rendering]
**Learning:** In scenarios involving rendering fixed-size pixel images to the framebuffer, falling back to pixel-by-pixel assignments using a nested loop limits performance. When 32bpp framebuffers are in use, the entire image can be copied much faster row-by-row utilizing `memcpy` for block memory transfers when no pixel blending or scaling is involved.
**Action:** When rendering solid pixel maps without alpha channel, use `memcpy` to copy rows directly if the destination BPP matches the source data, bypassing inner loops and significantly reducing CPU instructions.
## 2026-12-05 - [Duplicate Formatting Logic]
**Learning:** Re-implementing formatting functions (like `vsnprintf` and `itoa`) for specific modules (e.g., `klog`) can bypass highly optimized versions already present in the codebase. The custom implementation was significantly slower due to missing fast-paths (e.g., base 10/16 optimization) and backwards buffer filling techniques found in `kernel/stdio.c`.
**Action:** When working on formatting or string processing, check for and reuse existing, highly optimized core library implementations (`vsnprintf`) rather than duplicating slower, simplistic versions.

## 2026-03-08 - Optimize unrolled alpha-blend pixel loop & index bug
**Learning:** In unrolled rendering loops (e.g., `draw_window`), blindly using multiple memory array accesses `if (src[j]) dest[j] = src[j]` causes duplicate memory loads due to potential compiler pointer aliasing constraints. Furthermore, mixing loop-indexed writes (`dest[j]`) with manual pointer increments (`dest++`) inside a remainder loop causes out-of-bounds corruption and visual tearing since the target steps twice per iteration.
**Action:** Always cache array values into local scalar variables (`uint32_t c = src[j]; if (c) dest[j] = c;`) in highly iterated render passes. Never mix indexed addressing (`[j]`) with pointer arithmetic (`++`) on the same array in the same loop logic.

## 2026-03-08 - Pre-existing test failures
**Learning:** `tests/test_readv_security.c` fails to compile because it's missing a mock for `framebuffer_get_buffer`. This is a pre-existing test infrastructure issue unrelated to our window drawing optimizations.
**Action:** Ignored since it is unrelated to current modifications.

## 2024-11-20 - [Alpha Rendering Loop Optimization]
**Learning:** In tight 32bpp unrolled drawing loops (like the `draw_window` alpha path), using a combined bitwise OR on multiple pixels (e.g., `c0 | c1 | c2 ...`) to skip evaluating non-zero branches for entirely transparent chunks yields measurable performance wins, especially when dealing with rounded or sparse elements where long spans of zero-alpha exist. This relies on the property that any non-zero color value will result in a non-zero OR sum.
**Action:** When writing fast-paths for rasterizing sprites or windows with alpha channels, consider grouping pixels into unrolled chunks (e.g. 4x or 8x) and gating the memory write block with a single boolean check on their bitwise OR to avoid excessive branching.

## 2026-03-09 - [Render Fallback Loop Pointer Math]
**Learning:** In fallback rendering paths (e.g., legacy BPP support for windows or images), per-pixel math like `buffer[y * width + x]` adds massive overhead inside nested `y`/`x` loops.
**Action:** When a fallback path cannot utilize block memory operations (`memcpy`), hoist coordinate calculations out of the inner loop and use simple linear pointer increments (`*src++`) whenever drawing contiguous pixel regions.
