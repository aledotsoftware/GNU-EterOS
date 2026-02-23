import os
import time
from playwright.sync_api import sync_playwright

def verify_escape_key():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # Wait for boot splash to detach
        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
            print("Boot splash detached.")
        except Exception as e:
            print(f"Boot splash did not detach: {e}")

        # Open Launcher
        print("Opening launcher...")
        launcher_btn = page.locator("#launcher-trigger")
        launcher_btn.click()

        # Wait for launcher to be active
        page.wait_for_selector("#launcher.active")
        print("Launcher opened.")

        # Type in search
        print("Typing in search...")
        search_input = page.locator("#launcher-search")
        search_input.fill("GIMP")

        # Screenshot 1: Search with text
        page.screenshot(path="verification/1_search_with_text.png")

        # Press Escape (should clear search)
        print("Pressing Escape (should clear search)...")
        page.keyboard.press("Escape")

        # Wait a bit for potential JS execution
        time.sleep(0.5)

        # Verify input is empty
        input_value = search_input.input_value()
        if input_value == "":
            print("SUCCESS: Input is empty after Escape.")
        else:
            print(f"FAILURE: Input is not empty. Value: '{input_value}'")
            browser.close()
            exit(1)

        # Screenshot 2: Search cleared
        page.screenshot(path="verification/2_search_cleared.png")

        # Press Escape again (should close launcher)
        print("Pressing Escape again (should close launcher)...")
        page.keyboard.press("Escape")

        # Wait for launcher to close
        try:
            page.wait_for_selector("#launcher:not(.active)", timeout=2000)
            print("SUCCESS: Launcher is closed after second Escape.")
        except Exception as e:
            print(f"FAILURE: Launcher is still open. {e}")
            browser.close()
            exit(1)

        # Verify focus is back on trigger (optional but nice)
        is_trigger_focused = launcher_btn.evaluate("el => document.activeElement === el")
        if is_trigger_focused:
            print("SUCCESS: Focus returned to trigger.")
        else:
            print("WARNING: Focus did not return to trigger.")

        # Screenshot 3: Launcher closed
        page.screenshot(path="verification/3_launcher_closed.png")

        browser.close()

if __name__ == "__main__":
    verify_escape_key()
