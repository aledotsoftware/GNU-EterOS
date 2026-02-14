## 2026-05-23 - Unsafe String Conversions in Kernel
**Vulnerability:** Potential buffer overflow in `itoa` usage within `kmain`. `version_str[4]` could be overflowed if version numbers increase, as `itoa` lacked bounds checking.
**Learning:** Standard C library functions like `itoa` (or non-standard ones commonly used) often lack buffer size parameters, making them unsafe for kernel development where memory corruption is fatal.
**Prevention:** Always implement and use "safe" versions of string functions (suffixed with `_s` or similar) that take the destination buffer size as an argument, and enforce truncation or error handling.

## 2026-05-24 - Integer Overflow in Heap Allocation
**Vulnerability:** `kmalloc(SIZE_MAX)` caused `align(size)` to wrap around to 0 due to integer overflow, resulting in a valid pointer to a 0-byte block. Writing to this block corrupted the heap metadata of adjacent blocks.
**Learning:** Memory allocators must validate that requested size plus overhead (alignment, headers) does not exceed `SIZE_MAX`. Simple checks like `size > 0` are insufficient.
**Prevention:** Explicitly check for overflow before any size calculations: `if (size > SIZE_MAX - overhead) return NULL;`. Also validate against total available memory.
