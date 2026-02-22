import os
import time
from playwright.sync_api import sync_playwright

def verify_boot_splash_focus():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        print("Waiting for boot splash to finish...")

        # Wait for the splash screen to be removed
        # The total time is 2500ms (wait) + 600ms (fade) = 3100ms
        # We'll wait a bit longer to be safe
        page.wait_for_selector("#boot-splash", state="detached", timeout=5000)
        print("Boot splash removed.")

        # Check active element
        active_element = page.evaluate("document.activeElement.className")
        print(f"Active element class: '{active_element}'")

        if "os-logo" in active_element:
            print("SUCCESS: Focus is on os-logo.")
        else:
            print(f"FAILURE: Focus is NOT on os-logo. It is on '{active_element}'.")
            # Take screenshot for debugging
            page.screenshot(path="verification/boot_splash_focus_fail.png")
            browser.close()
            exit(1)

        browser.close()

if __name__ == "__main__":
    verify_boot_splash_focus()
