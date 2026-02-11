## 2026-05-23 - Unsafe String Conversions in Kernel
**Vulnerability:** Potential buffer overflow in `itoa` usage within `kmain`. `version_str[4]` could be overflowed if version numbers increase, as `itoa` lacked bounds checking.
**Learning:** Standard C library functions like `itoa` (or non-standard ones commonly used) often lack buffer size parameters, making them unsafe for kernel development where memory corruption is fatal.
**Prevention:** Always implement and use "safe" versions of string functions (suffixed with `_s` or similar) that take the destination buffer size as an argument, and enforce truncation or error handling.
