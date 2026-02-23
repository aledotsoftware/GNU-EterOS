import os
from playwright.sync_api import sync_playwright

def verify_search_optimization():
    print("Starting search optimization verification...")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # Wait for boot splash to disappear
        print("Waiting for boot splash...")
        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=5000)
            print("Boot splash disappeared.")
        except:
             print("Boot splash timeout.")

        # Open Launcher
        print("Opening launcher...")
        launcher_btn = page.locator("#launcher-trigger")
        launcher_btn.click()
        page.wait_for_selector("#launcher.active")

        # Test Case 1: Search for "GIMP"
        print("Testing search for 'GIMP'...")
        search_input = page.locator("#launcher-search")
        search_input.fill("GIMP")

        # Verify GIMP is visible
        gimp_item = page.locator(".launcher-item:has-text('GIMP')")
        if gimp_item.is_visible():
            print("PASS: GIMP is visible.")
        else:
            print("FAIL: GIMP is NOT visible.")
            exit(1)

        page.screenshot(path="verification/search_gimp.png")

        # Verify other items are hidden (sample check)
        spotify_item = page.locator(".launcher-item:has-text('Spotify')")
        if not spotify_item.is_visible():
            print("PASS: Spotify is hidden.")
        else:
            print("FAIL: Spotify should be hidden.")
            exit(1)

        # Test Case 2: Search for "Android" tag
        print("Testing search for 'Android' tag...")
        search_input.fill("Android")

        # Verify Android items are visible
        android_items = page.locator(".launcher-item:has(.and-tag)")
        count = android_items.count()
        visible_count = 0
        for i in range(count):
            if android_items.nth(i).is_visible():
                visible_count += 1

        if visible_count > 0:
            print(f"PASS: {visible_count} Android items visible.")
        else:
             print("FAIL: No Android items visible.")
             exit(1)

        page.screenshot(path="verification/search_android.png")

        # Verify Linux items are hidden
        linux_item = page.locator(".launcher-item:has-text('GIMP')")
        if not linux_item.is_visible():
            print("PASS: Linux item (GIMP) is hidden.")
        else:
            print("FAIL: Linux item (GIMP) should be hidden.")
            exit(1)

        # Test Case 3: Clear search
        print("Testing clear search...")
        search_input.fill("")

        # Verify all items are visible again
        all_items = page.locator(".launcher-item")
        count = all_items.count()
        visible_count = 0
        for i in range(count):
            if all_items.nth(i).is_visible():
                visible_count += 1

        if visible_count == count:
             print(f"PASS: All {visible_count} items visible after clear.")
        else:
             print(f"FAIL: Only {visible_count}/{count} items visible after clear.")
             exit(1)

        browser.close()
        print("All search verification tests passed!")

if __name__ == "__main__":
    verify_search_optimization()
