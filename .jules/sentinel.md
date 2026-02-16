## 2026-05-23 - Unsafe String Conversions in Kernel
**Vulnerability:** Potential buffer overflow in `itoa` usage within `kmain`. `version_str[4]` could be overflowed if version numbers increase, as `itoa` lacked bounds checking.
**Learning:** Standard C library functions like `itoa` (or non-standard ones commonly used) often lack buffer size parameters, making them unsafe for kernel development where memory corruption is fatal.
**Prevention:** Always implement and use "safe" versions of string functions (suffixed with `_s` or similar) that take the destination buffer size as an argument, and enforce truncation or error handling.

## 2026-05-24 - Integer Overflow in Heap Allocation
**Vulnerability:** `kmalloc(SIZE_MAX)` caused `align(size)` to wrap around to 0 due to integer overflow, resulting in a valid pointer to a 0-byte block. Writing to this block corrupted the heap metadata of adjacent blocks.
**Learning:** Memory allocators must validate that requested size plus overhead (alignment, headers) does not exceed `SIZE_MAX`. Simple checks like `size > 0` are insufficient.
**Prevention:** Explicitly check for overflow before any size calculations: `if (size > SIZE_MAX - overhead) return NULL;`. Also validate against total available memory.

## 2026-05-25 - Missing User Pointer Validation in Syscalls
**Vulnerability:** System calls like `read`, `write`, and `open` accepted raw user-space pointers and dereferenced them directly without checking if they pointed to valid user memory. A malicious user program could pass a kernel address (e.g., `0xFFFFFFFF...`) to `sys_read` to overwrite kernel code or `sys_write` to leak kernel data.
**Learning:** In a monolithic kernel running in Ring 0, the CPU does not automatically prevent the kernel from accessing kernel memory on behalf of a user. The kernel must explicitly validate that pointers provided by the user actually point to the user's address space.
**Prevention:** Implement a `validate_user_buffer` function that walks the page tables to verify `PAGE_USER` permissions for the entire buffer range, and use it in every syscall entry point that accepts a pointer.
