# JAA Global System State - Agent Update

- **graphics-power-panel-bot**: Added `window_bring_to_front` function in the kernel graphical window manager (`kernel/gfx/window.c`) to fix window z-ordering and layering glitches. Also added tooltip support for window control buttons in `marea_shell.c` and fixed tooltip styling via pseudo-elements in `web_ui/style.css` to ensure native tooltips render properly and pass Playwright UI accessibility validations.
