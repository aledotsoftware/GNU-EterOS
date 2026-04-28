import os
from playwright.sync_api import sync_playwright

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Get absolute path to index.html
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"

        print(f"Navigating to: {file_path}")
        page.goto(file_path)

        # Wait for splash screen to disappear (it has a 2.5s timeout + 0.6s transition)
        print("Waiting for splash screen to disappear...")
        page.wait_for_timeout(3500)

        # Open Control Center
        print("Opening Control Center...")
        cc_trigger = page.locator("#cc-trigger")
        cc_trigger.click()

        # Wait for animation
        page.wait_for_timeout(500)

        # Take screenshot of initial state (with notifications)
        print("Taking screenshot of notifications...")
        page.screenshot(path="verification/notifications_before.png")

        # Verify notifications are present
        notif_list = page.locator("#notif-list")
        notifs = notif_list.locator(".notif")
        count = notifs.count()
        print(f"Found {count} notifications.")

        if count == 0:
            print("ERROR: No notifications found!")
            browser.close()
            return

        # Find Clear button
        clear_btn = page.locator("#clear-notifs")
        if not clear_btn.is_visible():
            print("ERROR: Clear button not visible!")
            browser.close()
            return

        print("Clicking Clear button...")
        clear_btn.click()

        # Wait for update
        page.wait_for_timeout(200)

        # Verify empty state
        empty_state = page.locator("#notif-empty")
        if not empty_state.is_visible():
            print("ERROR: Empty state not visible after clearing!")
        else:
            print("SUCCESS: Empty state is visible.")

        # Verify list is empty
        notifs_after = notif_list.locator(".notif")
        if notifs_after.first.is_hidden():
            print("SUCCESS: Notification list is empty.")
        else:
            print(f"ERROR: Found {notifs_after.count()} notifications after clearing!")

        # Take screenshot of cleared state
        print("Taking screenshot of empty state...")
        page.screenshot(path="verification/notifications_after.png")

        browser.close()

if __name__ == "__main__":
    run()
