import os
import time
from playwright.sync_api import sync_playwright

def verify_boot_splash():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # 1. Verify Splash Screen is Visible
        splash = page.locator("#boot-splash")
        if splash.is_visible():
            print("Splash screen is visible.")
        else:
            print("ERROR: Splash screen is NOT visible.")
            browser.close()
            return

        # 2. Verify Background is White
        bg_color = splash.evaluate("el => getComputedStyle(el).backgroundColor")
        print(f"Splash background color: {bg_color}")
        if bg_color == "rgb(255, 255, 255)" or bg_color == "#ffffff":
            print("Background is white.")
        else:
            print(f"ERROR: Background is NOT white. It is {bg_color}")
            # Note: We proceed even if color is wrong, but report it.

        # Take screenshot of splash
        page.screenshot(path="verification/boot_splash_visible.png")

        # 3. Wait for Splash to Disappear
        # The app.js has 2500ms + 600ms = 3100ms total.
        # We wait for it to detach from DOM or become hidden.
        print("Waiting for splash screen to disappear...")
        try:
            splash.wait_for(state="detached", timeout=5000)
            print("Splash screen disappeared.")
        except Exception as e:
            print(f"ERROR: Splash screen did not disappear: {e}")
            browser.close()
            return

        # 4. Verify Focus is on Start Button (.os-logo)
        is_focused = page.evaluate("document.activeElement === document.querySelector('.os-logo')")
        if is_focused:
            print("SUCCESS: Focus is on Start Button (.os-logo).")
        else:
            focused_el = page.evaluate("document.activeElement.outerHTML")
            print(f"ERROR: Focus is NOT on Start Button. Active element: {focused_el}")

        # Take screenshot of desktop
        page.screenshot(path="verification/boot_desktop_ready.png")

        browser.close()

if __name__ == "__main__":
    verify_boot_splash()
