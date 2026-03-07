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

## 2025-05-18 - [IP Parsing Overflow]
**Vulnerability:** Integer overflow in `ip_aton` allowed bypassing IP filters or connecting to unintended hosts (e.g., 300.2.3.4 becoming valid).
**Learning:** Custom parsing logic often misses standard boundary checks present in libc functions like `inet_aton`.
**Prevention:** Always validate numeric inputs against their logical bounds (0-255 for octets) during parsing.

## 2026-05-26 - Recursive Pointer Validation in Scatter/Gather Syscalls
**Vulnerability:** `sys_readv` and `sys_writev` validated the individual buffers pointed to by `iov[i].iov_base`, but failed to validate the `iov` array itself (user pointer). A malicious user could pass a kernel address for `iov`, causing the kernel to read/write `struct iovec` from kernel memory or crash.
**Learning:** Complex syscalls with nested pointers (like scatter/gather I/O) require multiple layers of validation. Validating the "inner" pointers (buffers) is not enough; the "outer" container (array) must also be validated.
**Prevention:** Walk the data structure hierarchy and validate *every* pointer crossing the user/kernel boundary.

## 2026-05-27 - [Stale Data Reuse in mmap MAP_FIXED]
**Vulnerability:** `sys_mmap` with `MAP_FIXED` did not unmap existing pages at the target address. If the physical page was already present, `hal_mem_map` was skipped, leaving the old page (potentially with old data or wrong permissions) mapped instead of allocating a fresh zeroed page.
**Learning:** Memory management APIs must strictly enforce their contracts. `MAP_FIXED` implies replacement. Assuming "if mapped, it's fine" violates the guarantee of a fresh/zeroed page and can lead to data leaks or integrity violations.
**Prevention:** When implementing memory mapping overrides, always explicitly release the old resource (unmap & unref) before assigning the new one. Check for existence before allocation logic.

## 2026-05-28 - XSS in Internal UI Functions
**Vulnerability:** The `spawnApp` function in `web_ui/app.js` inserted the `name` argument directly into `innerHTML` without sanitization. Although current usages were safe (hardcoded strings), the function was exposed and could be exploited if ever called with user-controlled input (e.g., filenames).
**Learning:** Functions that generate HTML from arguments are dangerous even if "internal", as they become attack vectors when the codebase evolves or when they process indirect user input.
**Prevention:** Always escape HTML entities in string arguments before interpolating them into HTML templates, or use `textContent` where possible.

## 2026-05-29 - Implicit Pointer Types in IOCTL
**Vulnerability:** `sys_ioctl` blindly passed the `arg` parameter to drivers without validation, assuming drivers would handle it. However, because legacy IOCTLs like `TCGETS` do not encode the argument type in the request number, generic validation is impossible, and drivers running in kernel mode could inadvertently dereference a malicious user-supplied pointer.
**Learning:** When a system call multiplexes disparate operations (like `ioctl`), central validation is difficult. Absence of strict type encoding leads to "blind trust" chains where no layer validates the pointer.
**Prevention:** For legacy IOCTLs, explicitly check known request codes in the syscall handler and validate their specific argument types (size and direction) before dispatching to drivers. For new IOCTLs, strictly enforce encoded request numbers (e.g., `_IOR`, `_IOW`) that allow generic validation.

## 2026-03-01 - [sys_access Permission Bypass]
**Vulnerability:** The `sys_access` syscall returned success (0) merely if a file existed, ignoring the requested access modes (read, write, execute) and failing to check the calling task's UID/GID against the file's owner/group/mask.
**Learning:** In early-stage custom kernels, standard system calls often have stubbed implementations (like returning success without validation) that are easily overlooked and represent severe privilege escalation or information leakage vectors. System states must explicitly map user identity directly within core structures (like `task_t`).
**Prevention:** Audit all POSIX-compatible syscalls to ensure they don't just mimic the interface but actually enforce the underlying security constraints (e.g. evaluating bitwise permission masks). Ensure user identity (UID/GID) is built into core structures (task scheduler) and not hardcoded to 0 (root).
## 2024-05-19 - Enforce O_RDONLY and O_WRONLY in sys_read/sys_write
**Vulnerability:** `sys_read` and `sys_write` in `kernel/arch/x86_64/syscall.c` did not validate if a file descriptor was opened with the appropriate access modes (`O_RDONLY`, `O_WRONLY`, `O_RDWR`). A process could open a file as read-only but still successfully write to it using `sys_write`.
**Learning:** System calls that operate on file descriptors must independently verify the access permissions established during the `sys_open` call, rather than trusting the user program or assuming the file system layer will catch the violation.
**Prevention:** Always check `fd_table[fd].flags & O_ACCMODE` before performing read/write operations on a file descriptor to ensure the action is explicitly permitted.

## 2026-03-03 - [TCP Data Offset Integer Underflow]
**Vulnerability:** In `tcp_input`, an invalid TCP Data Offset (`doff`) value less than 5 (20 bytes) could result in an integer underflow when calculating `data_len` (`len - doff`). This could cause out-of-bounds reads and memory corruption when processing manipulated network packets.
**Learning:** Network header parsers are a critical entry point and highly susceptible to mathematical errors. Malformed packets with syntactically impossible values (like a header size smaller than the protocol's minimum) can exploit trust in calculated lengths to bypass subsequent bounds checks.
**Prevention:** Implement strict, defensive validation of all parsed header lengths (e.g., ensuring `doff >= 20` and `doff <= len`) at the very beginning of the packet processing function, before any pointer arithmetic or data length calculations occur.

## 2026-06-02 - Heap Allocation for Kernel Path Buffers
**Vulnerability:** System calls like `sys_faccessat` used a stack-allocated array (`char kpath[256];`) to store copied user paths, and then erroneously called `kfree(kpath)` upon return. This caused memory corruption or a kernel panic, representing a severe Denial of Service (DoS) risk.
**Learning:** In kernel development, buffers used for copying user-provided strings (like file paths) must be allocated on the heap (using `kmalloc`) to safely copy the data, prevent TOCTOU vulnerabilities, and allow for proper cleanup via `kfree`. Stack allocations shouldn't be freed via `kfree`, and large arrays can cause stack overflows.
**Prevention:** In kernel system calls, always allocate user string copies on the heap using `kmalloc`. Ensure that the allocation is checked for failure and that all early exit paths properly clean up the allocated memory using `kfree` to prevent memory leaks.
