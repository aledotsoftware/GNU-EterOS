import os
import time
from playwright.sync_api import sync_playwright

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        cwd = os.getcwd()
        file_path = f"file://{os.path.join(cwd, 'web_ui/index.html')}"
        print(f"Navigating to: {file_path}")
        page.goto(file_path)

        # 1. Open Launcher
        print("Opening Launcher...")
        page.locator("#dock .dock-item[aria-label='Lanzador de aplicaciones']").click()
        page.wait_for_selector("#launcher.active")
        time.sleep(1) # wait for animation

        # 2. Type search query
        print("Typing 'gimp'...")
        search_box = page.locator("#launcher-search")
        search_box.focus()
        search_box.fill("gimp")
        # Wait for filter
        time.sleep(0.5)

        # 3. Press ArrowDown to focus first item
        print("Pressing ArrowDown...")
        search_box.press("ArrowDown")

        # Verify focus moved to GIMP item
        focused_label = page.evaluate("document.activeElement.getAttribute('aria-label')")
        print(f"Focused Element: {focused_label}")

        if focused_label != "Abrir GIMP":
            print(f"FAIL: Focus did not move to GIMP. Currently focused: {focused_label}")
            exit(1)

        # 4. Press Enter to launch
        print("Pressing Enter to launch...")
        page.keyboard.press("Enter")

        # 5. Verify Window Appears
        print("Waiting for GIMP window...")
        try:
            page.wait_for_selector(".window-title:has-text('GIMP')", timeout=5000)
            print("SUCCESS: GIMP Window launched via keyboard!")
        except Exception as e:
            print("FAIL: Window did not appear.")
            exit(1)

        # Take screenshot for verification
        screenshot_path = "verification/launcher_nav_success.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {screenshot_path}")

        browser.close()

if __name__ == "__main__":
    run()
