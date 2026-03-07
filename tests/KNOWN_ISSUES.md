# Known Issues

This file documents known bugs, vulnerabilities, and testing issues in the éterOS kernel and userspace components.

## Resolved Issues

*   **Vulnerability: Unbounded `sys_readv` (DoS)**
    *   **Status:** Resolved
    *   **Description:** The `sys_readv` syscall did not enforce a limit on the number of `iovec` structures passed via the `iovcnt` parameter. This could lead to an attacker supplying a massive `iovcnt` to trigger a Denial of Service by causing an infinite loop or unbounded memory access inside kernel space.
    *   **Fix:** Added a limit to `iovcnt` to prevent DoS.
    *   **Regression Test:** `tests/test_readv_security.c` (Test 4: Large iovcnt).

*   **Vulnerability: Invalid `iov` pointers in `sys_readv`**
    *   **Status:** Resolved
    *   **Description:** The array of `iovec` structures passed from userspace to `sys_readv` was not validated using `vmm_verify_user_access`. A malicious user could pass kernel memory pointers in the `iov` array to force the kernel to overwrite sensitive data.
    *   **Fix:** Added pointer validation logic using `vmm_verify_user_access` for `iov` and its entries.
    *   **Regression Test:** `tests/test_readv_security.c` (Test 2).

*   **Bug: Missing TLB Flush in VMM Unmap**
    *   **Status:** Resolved
    *   **Description:** The VMM was failing to flush the translation lookaside buffer (TLB) when unmapping pages via `vmm_unmap_page`. This allowed userspace tasks to continue accessing freed memory because the CPU cached the mapping.
    *   **Fix:** Added `vmm_flush_tlb_local(virt)` immediately after updating the PTE.
    *   **Regression Test:** `tests/test_vmm_unmap.c`

*   **Bug: `sys_open` allowed write access to directories**
    *   **Status:** Resolved
    *   **Description:** The `sys_open` syscall did not properly check the `flags` against the node type, meaning a task could open a directory node with `O_WRONLY` or `O_RDWR` and corrupt it.
    *   **Fix:** Added a check for `FS_DIRECTORY` against write-mode flags. Returns `-EISDIR`.
    *   **Regression Test:** `tests/test_sys_open.c`

## Open Issues

*   (None currently tracked.)
