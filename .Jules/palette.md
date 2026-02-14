## 2026-05-27 - [Boot Splash Implementation]
**Learning:** For low-level kernel UIs, programmatic geometry (circles, lines) is often cleaner and more efficient than decoding bitmap assets for simple logos. It also scales perfectly to any resolution.
**Action:** When designing simple icons or logos for kernel mode, prefer geometric primitives over asset loading unless absolutely necessary.
