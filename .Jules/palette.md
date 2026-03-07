## 2026-03-06 - Clean Splash Screen Boot Sequence
**Learning:** To create a clean visual boot transition (like Windows/Android) in a bare-metal kernel without breaking timer loops, the terminal output must be temporarily silenced (`terminal_set_silent`) rather than moving the graphical rendering routine (`show_splash`) before interrupts are enabled. Moving `hlt`-dependent wait loops to early boot causes fatal system freezes.
**Action:** When adjusting boot UX sequences, explicitly verify that the rendering code does not rely on hardware subsystems (like the timer interrupt or VFS) that are initialized later in the boot pipeline.

## 2026-03-08 - Avoid Blinking Cursor in Hardware Idle Loops
**Learning:** In bare-metal applications utilizing hardware interrupt wait loops (`hlt`), implementing an active blinking cursor by repeatedly calling terminal output functions significantly degrades performance and floods secondary outputs (like serial ports). This is because the visual redraw becomes tightly coupled to the timer interrupt frequency instead of user input.
**Action:** Always prefer a static visual affordance (e.g., printing a cursor once before entering the `hlt` loop) rather than continuous redrawing in idle loops.
