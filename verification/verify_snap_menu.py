import os
import time
from playwright.sync_api import sync_playwright

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        # Resolve absolute path to web_ui/index.html
        cwd = os.getcwd()
        file_path = f"file://{os.path.join(cwd, 'web_ui/index.html')}"
        print(f"Navigating to: {file_path}")

        page.goto(file_path)

        # Wait for dock to be ready
        page.wait_for_selector(".dock-item")

        # Spawn an app (e.g., Terminal)
        print("Clicking Terminal icon...")
        page.locator(".dock-item[aria-label='Abrir Terminal']").click()

        # Wait for window to appear
        print("Waiting for window...")
        page.wait_for_selector(".window")

        # Find the maximize button
        maximize_btn = page.locator(".control.maximize")

        # Focus it
        print("Focusing Maximize button...")
        maximize_btn.focus()

        # Wait a bit for transition
        time.sleep(0.5)

        # Check snap menu visibility
        snap_menu = page.locator(".snap-menu")

        # Take screenshot
        screenshot_path = "verification/snap_menu_after.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {screenshot_path}")

        # Get computed style for opacity to be sure
        opacity = snap_menu.evaluate("el => getComputedStyle(el).opacity")
        pointer_events = snap_menu.evaluate("el => getComputedStyle(el).pointerEvents")

        print(f"Snap Menu Opacity: {opacity}")
        print(f"Snap Menu Pointer Events: {pointer_events}")

        is_visible_playwright = snap_menu.is_visible()
        print(f"Playwright is_visible(): {is_visible_playwright}")

        # Assertions
        if str(opacity) != "1":
            print("FAIL: Opacity is not 1")
            exit(1)

        if pointer_events != "all":
            print("FAIL: pointer-events is not 'all'")
            exit(1)

        print("SUCCESS: Snap Menu is visible and interactive!")

        browser.close()

if __name__ == "__main__":
    run()
