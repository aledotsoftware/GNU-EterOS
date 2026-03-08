import os
import time
from playwright.sync_api import sync_playwright

def verify_palette():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Open Notepad
        print("Opening Notepad...")
        page.locator(".icon").nth(1).click()

        # Wait for window to appear
        page.wait_for_selector('.window')

        # Focus Start button to see outline
        print("Focusing Start button...")
        page.locator("#start-btn").focus()
        # Ensure focus-visible is triggered. Programmatic focus might not trigger focus-visible in some cases without key presses.
        page.keyboard.press("Tab")
        page.keyboard.press("Shift+Tab")

        # Take screenshot of start button focused
        page.locator("#taskbar").screenshot(path="verification/start_btn_focus.png")

        # Focus window close button
        print("Focusing window close button...")
        close_btn = page.locator('.window-controls button')
        close_btn.focus()
        page.keyboard.press("Tab")
        page.keyboard.press("Shift+Tab")

        # Hover over window close button to show tooltip
        print("Hovering close button for tooltip...")
        close_btn.hover()

        # Wait for tooltip transition (0.2s)
        time.sleep(0.5)

        # Take a screenshot of the window header showing tooltip and focus
        page.locator('.window').screenshot(path="verification/window_close_tooltip.png")

        browser.close()
        print("Verification complete.")

if __name__ == "__main__":
    verify_palette()
