from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        # Load the local HTML file
        file_url = f"file://{os.path.abspath('web_ui/index.html')}"
        page.goto(file_url)

        # Wait for the boot splash to disappear
        print("Waiting for boot splash...")
        page.locator("#boot-splash").wait_for(state="detached", timeout=10000)

        # 0. Open the launcher
        print("Opening launcher...")
        page.locator("#launcher-trigger").click()
        page.wait_for_selector("#launcher.active")

        # 1. Click the calculator icon to open a window
        print("Clicking first launcher item...")
        page.locator(".launcher-item").first.click()

        # 2. Wait for window to appear
        page.wait_for_selector(".window")

        # Take a screenshot to verify the window opens and the close button has focus
        page.screenshot(path="verification/window_focused.png")

        # 3. Click the close button
        page.locator(".control.close").click()

        # Wait for the window to disappear
        page.wait_for_selector(".window", state="hidden")

        # Take another screenshot to verify it closed
        page.screenshot(path="verification/window_closed.png")

        browser.close()

if __name__ == "__main__":
    run()
