
import os
import time
from playwright.sync_api import sync_playwright

def verify_tooltips():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # specific to the environment, get absolute path
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"

        print(f"Navigating to {file_path}")
        page.goto(file_path)

        # Wait for boot splash to disappear
        print("Waiting for boot splash to disappear...")
        # Boot splash takes about 2.5s + 0.6s fade out
        page.wait_for_timeout(3500)

        # Spawn an app (GIMP)
        print("Spawning GIMP...")
        # The dock item for GIMP has class 'running' (it's pre-marked as running in HTML but spawning logic handles it)
        # Actually, clicking the dock item spawns it.
        # Find dock item with aria-label="Abrir GIMP"
        page.click('[aria-label="Abrir GIMP"]')

        # Wait for window to appear
        page.wait_for_selector('.window')

        # Find the window controls
        # We want to hover over the 'minimize' control
        minimize_btn = page.locator('.window .control.minimize')

        print("Hovering over minimize button...")
        minimize_btn.hover()

        # Wait for tooltip transition (0.2s)
        page.wait_for_timeout(500)

        # Take a screenshot of the window header
        header = page.locator('.window .window-header')

        # We need to capture enough area to see the tooltip below the header
        # The tooltip is absolutely positioned relative to the control, which is inside the header.
        # But overflow might be an issue if the header clips content.
        # Let's check style.css for .window-header overflow.
        # .window-header { ... } doesn't specify overflow.
        # .window { ... overflow: visible; ... }
        # So it should be visible.

        # Capture the whole window to be safe
        window_el = page.locator('.window')

        output_path = f"{cwd}/verification/verify_tooltip.png"
        print(f"Taking screenshot to {output_path}")
        window_el.screenshot(path=output_path)

        browser.close()

if __name__ == "__main__":
    verify_tooltips()
