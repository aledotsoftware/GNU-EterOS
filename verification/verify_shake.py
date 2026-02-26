from playwright.sync_api import sync_playwright
import time
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()
        page = context.new_page()

        # Load the local HTML file
        cwd = os.getcwd()
        page.goto(f"file://{cwd}/web_ui/index.html")

        print("Waiting for boot splash...")
        # Reduce wait time by injecting JS to remove splash if needed, but let's wait normally
        page.wait_for_timeout(3500)

        # Wait for dock to appear
        page.wait_for_selector("#dock", state="visible")

        print("Opening GIMP first time...")
        # Use a precise selector for the dock item
        page.locator(".dock-item[aria-label='Abrir GIMP']").click()

        # Wait for window to appear
        window = page.locator(".window").first
        window.wait_for(state="visible")
        print("Window opened.")

        time.sleep(1) # Wait for animation to settle

        page.screenshot(path="verification/before_shake.png")

        print("Opening GIMP second time to trigger shake...")
        page.locator(".dock-item[aria-label='Abrir GIMP']").click()

        # Check if class is added immediately
        # We need to act fast or use page.eval to check
        is_shaking = page.evaluate("""() => {
            const win = document.querySelector('.window');
            return win.classList.contains('shake');
        }""")

        print(f"Is shaking immediately? {is_shaking}")

        if is_shaking:
            page.screenshot(path="verification/shake_active.png")
            print("Captured shake_active.png")

        # Wait for animation to finish (500ms)
        time.sleep(0.6)

        is_shaking_after = page.evaluate("""() => {
            const win = document.querySelector('.window');
            return win.classList.contains('shake');
        }""")

        print(f"Is shaking after 600ms? {is_shaking_after}")

        if not is_shaking and is_shaking_after:
             print("Something is wrong with timing.")

        browser.close()

if __name__ == "__main__":
    run()
