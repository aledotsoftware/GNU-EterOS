## 2024-05-23 - [Memmove Corruption & Host Testing]
**Learning:** `memmove` implementation contained a redundant C loop after an optimized inline ASM block, causing double-copying and memory corruption in backward copy path.
**Action:** When optimizing string functions with inline ASM, verify that no fallback/redundant logic remains active that could conflict with the ASM state (especially pointer updates).

**Learning:** Host-based testing of kernel code (`tests/*.c`) requires careful include management. Using `-Iinclude` can shadow system headers (`<time.h>`, `<stdio.h>`) with kernel headers, causing redefinition errors.
**Action:** Do not use `-Iinclude` for host tests if they rely on standard library features. Instead, rely on relative paths or selective includes, and ensure `__ETEROS_HOST_TEST__` is defined to trigger compatibility layers.

## 2024-05-24 - [Frontend Layout Thrashing]
**Learning:** The `web_ui` prototype heavily relies on vanilla JS with repeated DOM queries (`querySelectorAll`, `innerText`) inside tight loops (e.g., search input), causing layout thrashing.
**Action:** Always cache DOM elements and their text content into a JS object on first load or initialization for filter/search operations, to decouple logical filtering from DOM read/write cycles.
