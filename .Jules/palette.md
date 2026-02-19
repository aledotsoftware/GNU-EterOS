## 2026-02-15 - [The Logo as an Anchor]
**Learning:** Logos in the status bar often serve as "Home" or "Start" buttons. Without hover feedback or a tooltip, they remain "mystery meat" navigation elements. Adding a subtle highlight and a functional hint (e.g., "HUB") transforms a branding element into a clear interactive anchor.
**Action:** Always provide hover affordance for branding elements that double as system-level controls.

## 2026-05-27 - [Boot Splash Implementation]
**Learning:** For low-level kernel UIs, programmatic geometry (circles, lines) is often cleaner and more efficient than decoding bitmap assets for simple logos. It also scales perfectly to any resolution.
**Action:** When designing simple icons or logos for kernel mode, prefer geometric primitives over asset loading unless absolutely necessary.

## 2026-02-14 - [Accessible Hover Feedback]
**Learning:** Strobing "active" indicators (blinking borders) can be disorienting and inaccessible (seizure risk/cognitive load). A steady glow or solid highlight provides clearer feedback and feels more premium.
**Action:** Replace `pulse % 2` logic with steady state or smooth transitions for hover effects in UI components.

## 2026-02-14 - [Real World Clocks]
**Learning:** Users instinctively look at the top-right corner for "Current Time", not "Uptime". Displaying uptime in a HH:MM format confuses the mental model of a desktop environment.
**Action:** Always default to Wall Clock Time (RTC) in status bars, and relegate uptime to SysInfo or tooltips.

## 2026-05-28 - [Invisible Information Layers]
**Learning:** In space-constrained kernel UIs, tooltips on hover effectively declutter the persistent interface while keeping detailed information accessible. Using a simple "hover check -> render overlay" pattern is cheap and intuitive.
**Action:** Consider implementing a generic `ui_tooltip(rect, text)` helper for future components.

## 2026-05-29 - [Text Input Affordance]
**Learning:** A text input without a cursor (caret) feels like a static label, confusing users about whether it's editable. A simple blinking vertical bar creates immediate affordance for typing.
**Action:** Always include a visual cursor in focused text fields, even in minimal kernel GUIs.

## 2026-05-30 - [Living Focus States]
**Learning:** A slow, breathing pulse (2s period) for focus states communicates "active/ready" without the cognitive load of rapid blinking. It aligns with the "Influence Field" metaphor of Flux UI.
**Action:** Use smooth alpha modulation for focus indicators instead of on/off blinking.

## 2026-06-05 - [Connectivity Status Visibility]
**Learning:** In a connected OS, users expect immediate feedback on network status. Hiding connectivity details inside a "Settings" app adds friction. A simple status bar icon with a tooltip (showing IP or "Offline") provides reassurance and quick diagnostics without clutter.
**Action:** Ensure critical system states (Network, Battery, Audio) are always visible in the global status bar with detailed tooltips.

## 2026-10-24 - [Keyboard Accessibility for Iconic Launchers]
**Learning:** A grid of app icons without keyboard support (tabindex/role) completely blocks keyboard-only users. Adding simple focus styles (scale + glow) and standard keyboard events transforms a "touch-first" launcher into a fully accessible desktop experience.
**Action:** Always ensure interactive grids (like launchers or galleries) support arrow/tab navigation and have clear `:focus-visible` states.
