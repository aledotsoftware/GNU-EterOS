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

## 2026-11-15 - [The Invisible Tab Trap]
**Learning:** Placing interactive elements inside a container that is only revealed on hover creates a "black hole" for keyboard users. They tab into the container, focus disappears, and they are trapped in invisible controls.
**Action:** Use `:focus-within` on the parent container to reveal hidden interactive children, ensuring keyboard users can see what they are navigating.

## 2026-12-05 - [Focus Management in Web-based OS]
**Learning:** Standard web focus flow (tabbing sequentially) breaks the illusion of a desktop environment. Explicitly managing focus (e.g., auto-focusing menu items on open, handling Escape to return focus) is critical to make the interface feel like an OS, not just a webpage.
**Action:** Always implement explicit focus entry/exit logic for overlay components (menus, windows, launchers).

## 2026-02-20 - [Empty States are Helpful States]
**Learning:** When a search yields no results, a blank screen feels like a system failure. Providing an explicit "No results" message with a helpful tip transforms a dead end into a navigational guide.
**Action:** Always implement empty states for search/filter interfaces to maintain user confidence.

## 2027-02-18 - [CSS-Only Tooltips]
**Learning:** Using `attr(data-tooltip)` with `::before` pseudo-elements allows for lightweight, accessible tooltips without JavaScript overhead, keeping the interface snappy and declaratively defined.
**Action:** Standardize `data-tooltip` usage across icon-only controls to provide consistent contextual help.

## 2027-02-17 - [Dynamic ARIA States for Custom Panels]
**Learning:** Static ARIA attributes on custom trigger elements (like a control center button) fail to communicate state changes to assistive technology. Explicitly toggling `aria-expanded` in JS when the panel opens/closes is crucial for a complete accessibility experience.
**Action:** Always pair custom UI toggles with dynamic `aria-expanded` state management in the event handler.

## 2027-03-01 - [Boot Splash Focus Handoff]
**Learning:** Transitions between a loading state (like a boot splash) and the main interface can disorient keyboard/screen reader users if focus isn't explicitly moved to a logical starting point.
**Action:** Always programmatically set focus to the primary interactive element (e.g., Start button) after a major state transition or splash screen removal.

## 2027-04-12 - [Search Accessibility & Escape]
**Learning:** For overlay search interfaces (like a launcher), users expect the "Escape" key to perform context-sensitive actions: clearing text first, then closing the overlay. This two-stage dismissal pattern prevents accidental closure and aligns with desktop OS conventions.
**Action:** Implement a `keydown` handler on search inputs that checks value length: `Escape` -> `value.length > 0 ? clear() : close()`.

## 2027-04-14 - [Actionable Empty States]
**Learning:** An empty state that only says "No results" is a dead end. Adding a clear, primary action button (like "Clear Search" or "Create New") turns a failure state into a recovery opportunity, keeping the user in the flow.
**Action:** Always include a recovery action button in empty state components.

## 2026-02-24 - [Dynamic Control Center Values]
**Learning:** Adding real-time data (like current date and slider percentage values) to previously static or abstract UI elements (like sliders) significantly improves user confidence and system liveliness without cluttering the interface.
**Action:** When working on "Control Center" or "Settings" panels, always look for opportunities to expose the actual value of a control or the current system state (time, date, battery %) rather than relying on abstract visual indicators alone.
