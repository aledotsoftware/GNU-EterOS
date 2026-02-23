import os
import time
from playwright.sync_api import sync_playwright

def verify_boot_splash_visual():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        print("Waiting for boot splash...")
        # Wait for the splash to be visible
        page.wait_for_selector("#boot-splash")

        # Wait a bit for the animation to start/be in progress
        time.sleep(1)

        # Take screenshot of the splash screen
        screenshot_path = "verification/boot_splash_visual.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {screenshot_path}")

        browser.close()

if __name__ == "__main__":
    verify_boot_splash_visual()
