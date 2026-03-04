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

## 2026-12-05 - [String Copy Bulk Operations]
**Learning:** Manual byte-by-byte loops for standard library string functions like `strncpy` are highly inefficient compared to utilizing block memory operations. The old implementation iterated twice per character (copy, then pad).
**Action:** Replace byte-by-byte loops in string manipulation primitives with combinations of optimized operations (`strnlen`, `memcpy`, `memset`). This allows the underlying architecture to use hardware fast-string operations (e.g., `rep movsq`, `rep stosq`), yielding >20x performance improvements for large strings.
## 2026-12-05 - [gfx_draw_rect Optimized Fast Path]
**Learning:** Drawing hollow rectangles pixel-by-pixel using Bresenham's line algorithm (`gfx_draw_line`) is extremely slow and inefficient when the lines are strictly horizontal and vertical. It incurs massive overhead due to repeated bound checks and single-pixel writes.
**Action:** When drawing axis-aligned hollow rectangles (e.g. `gfx_draw_rect`), always decompose them into 4 solid rectangle fills (`gfx_fill_rect`) representing the top, bottom, left, and right borders. This leverages highly optimized `memset32` fast-paths and direct memory access underneath.
## 2024-05-18 - Fast paths for basic graphics primitives
**Learning:** In a software-rendered UI compositor, seemingly complex high-level draw functions (like drawing window borders via `gfx_draw_rect`) often boil down to the simplest possible primitives: pure horizontal and vertical lines. Bresenham's line algorithm has massive branching overhead. Bypassing it for these simple primitives enables using highly optimized block memory copies (`memset32`).
**Action:** When working on graphics rendering code, always identify the most common degenerate cases (like straight lines or opaque solid rectangles) and create fast paths that bypass the general algorithmic logic in favor of bulk memory operations.
## 2026-12-05 - [Fast-Path Rectangle Outline]
**Learning:** Drawing a rectangle's outline by executing Bresenham's line algorithm (`gfx_draw_line`) 4 times generates a massive amount of overhead for simple vertical and horizontal lines. Setting individual pixels limits bandwidth use drastically.
**Action:** When a rectangle outline needs to be drawn (`gfx_draw_rect`), do so by composing it out of 4 independent memory blocks via fast-path `gfx_fill_rect` logic, resulting in roughly a 4-5x speed improvement and avoiding loop overhead.
