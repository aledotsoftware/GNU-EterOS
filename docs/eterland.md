# Eterland Compositor

`eterland.c` is the userspace graphical compositor and window manager for eterOS.

## Architecture

Eterland operates as a privileged userspace application. It interacts with the kernel through several interfaces:

- **/dev/dri/card0** (or **/dev/fb0**): Mapped via `mmap()` to obtain a direct pointer to the linear framebuffer video memory. Eterland renders all windows, cursors, and UI elements directly into this buffer.
- **/dev/input/mouse0**: Read to obtain mouse movement (`EV_REL`) and button click (`EV_KEY`) events.
- **/dev/tty** or keyboard drivers: Read for keystrokes to pass to focused windows.
- **/dev/shm/**: Shared memory file system used to share pixel buffers between client applications and the compositor (Zero-copy IPC).

## Window Management

Eterland manages a list of `window_t` structures. It handles:
- **Z-Ordering**: The array of windows dictates rendering order. Focused windows are moved to the top (end of the array).
- **Clipping**: To optimize rendering, Eterland calculates dirty rectangles and clips drawing operations so that background windows don't draw over foreground windows unnecessarily.
- **Compositing**: Client applications draw into their private shared memory buffers. Eterland composites these buffers onto the main framebuffer, handling alpha blending and window decorations (title bars, borders, buttons).

## Rendering Pipeline

1. **Input Polling**: The main loop polls (`sys_poll`) for input events.
2. **Event Dispatch**: Mouse clicks evaluate window hitboxes (focus, drag, close). Keystrokes are sent to the focused window's message queue.
3. **Dirty Region Calculation**: Areas that need updating (due to cursor movement or window changes) are marked.
4. **Drawing**:
    - Restore background where the cursor used to be.
    - Draw windows intersecting the dirty regions.
    - Draw the cursor at the new position.
5. **Present**: (If double buffering is used, swap buffers here).

## Android/Linux Compatibility

Eterland aims to eventually support Wayland or SurfaceFlinger protocols to act as a seamless compositor for ported Linux/Android GUI applications.
