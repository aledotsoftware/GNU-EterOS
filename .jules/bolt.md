## 2026-02-14 - [Cached System Stats]
**Learning:** Calculating memory statistics involves 64-bit division `(total - free) / (1024*1024)`. Doing this every frame (60Hz) is unnecessary for values that change slowly.
**Action:** Cache UI indicators that require expensive math (div/mod) and update them at a lower frequency (e.g., 2Hz or 1Hz) to spare CPU cycles for the render loop.
