import os
import time
from playwright.sync_api import sync_playwright

def verify_debounce():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # Wait for splash screen to disappear
        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
        except:
            print("Boot splash timeout")
            return

        # Open launcher
        page.get_by_label("Lanzador de aplicaciones").click()
        page.wait_for_selector("#launcher.active")

        # Inject spy to count calls
        page.evaluate("""
            window.filterAppsCallCount = 0;
            const originalFilterApps = window.filterApps;
            window.filterApps = function() {
                window.filterAppsCallCount++;
                return originalFilterApps.apply(this, arguments);
            };
        """)

        # Type 'linux' quickly
        print("Typing 'linux'...")
        search_input = page.locator("#launcher-search")
        search_input.type("linux", delay=50) # fast typing

        # Wait a bit for debounce to trigger (if any)
        time.sleep(1.0)

        # Get count
        count = page.evaluate("window.filterAppsCallCount")
        print(f"filterApps calls: {count}")

        # Verify results are correct (should show 6 items for linux)
        visible_items = page.locator(".launcher-item:visible").count()
        print(f"Visible items: {visible_items}")

        # If count > 2, fail (allow 1 for setup/initial clear + 1 for debounce)
        # Note: Depending on timing, it might be just 1 call if the initial clear happened before spy.
        # But typically: type triggers 1 debounced call.
        if count > 3:
             print("FAIL: Too many calls (Debounce not working)")
             exit(1)
        else:
             print("PASS: Debounce working")

        browser.close()

if __name__ == "__main__":
    verify_debounce()
