from playwright.sync_api import sync_playwright, expect
import os
import time

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading: {file_path}")
        page.goto(file_path)

        # Wait for boot splash
        page.wait_for_timeout(3000)

        # Open Launcher
        print("Opening Launcher...")
        launcher_trigger = page.locator("#launcher-trigger")
        launcher_trigger.click()

        # Wait for animation
        page.wait_for_timeout(500)

        # Verify launcher is active
        launcher = page.locator("#launcher")
        expect(launcher).to_have_class("launcher active")

        # Type in search
        search_input = page.locator("#launcher-search")
        print("Typing 'firefox'...")
        search_input.fill("firefox")

        # Wait for debounce (150ms)
        page.wait_for_timeout(300)

        # Check filtering
        # Firefox should be visible
        firefox = page.locator(".launcher-item").filter(has_text="Firefox")
        expect(firefox).to_be_visible()

        # GIMP should be hidden
        gimp = page.locator(".launcher-item").filter(has_text="GIMP")
        expect(gimp).to_be_hidden()

        print("Filtering successful!")

        # Take screenshot
        page.screenshot(path="verification/launcher_verification.png")
        print("Screenshot saved to verification/launcher_verification.png")

        browser.close()

if __name__ == "__main__":
    run()
