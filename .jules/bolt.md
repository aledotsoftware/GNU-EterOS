## 2026-02-14 - [Cached System Stats]
**Learning:** Calculating memory statistics involves 64-bit division `(total - free) / (1024*1024)`. Doing this every frame (60Hz) is unnecessary for values that change slowly.
**Action:** Cache UI indicators that require expensive math (div/mod) and update them at a lower frequency (e.g., 2Hz or 1Hz) to spare CPU cycles for the render loop.

## 2026-10-14 - [Word-at-a-time String Comparison]
**Learning:** When optimizing `memcmp` with word-sized (e.g., `uint64_t`) comparisons on Little Endian architectures (x86_64), the integer comparison result (`a < b` or `a > b`) does NOT necessarily match the lexicographical byte-wise comparison result due to byte significance order.
**Action:** Use word-sized comparisons ONLY to check for equality (`a == b`). Upon finding the first differing word, fall back to a byte-wise comparison loop to determine the correct return value sign.
