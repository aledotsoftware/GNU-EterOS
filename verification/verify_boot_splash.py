
from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Get absolute path to index.html

        file_path = "file:///app/web_ui/index.html"

        print(f"Navigating to {file_path}")
        page.goto("file:///app/web_ui/index.html")

        # Check if boot splash is visible immediately
        splash = page.locator("#boot-splash")
        if splash.is_visible():
            print("Boot splash is visible.")
            page.screenshot(path="verification/boot_splash_initial.png")
        else:
            print("Boot splash is NOT visible initially!")

        # Wait for splash to disappear (app.js has 2500ms delay + 600ms transition)
        # We'll wait 4 seconds to be safe
        print("Waiting for boot sequence...")
        page.wait_for_timeout(4000)

        if not splash.is_visible():
            print("Boot splash disappeared as expected.")
            page.screenshot(path="verification/boot_splash_after.png")
        else:
            print("Boot splash is STILL visible!")

        # Verify focus moved to OS logo (Start button) for accessibility
        active_element = page.evaluate("document.activeElement.className")
        if "os-logo" in active_element:
            print("Accessibility Check Passed: Focus moved to OS logo.")
        else:
            print(f"Accessibility Check FAILED: Focus is on '{active_element}', expected 'os-logo'.")

        browser.close()

if __name__ == "__main__":
    run()
