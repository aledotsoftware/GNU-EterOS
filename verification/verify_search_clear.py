from playwright.sync_api import sync_playwright
import os
import time

def run():
    print("Starting verification script...")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        file_path = f"file://{os.getcwd()}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Click the launcher button in the dock
        print("Clicking launcher...")
        page.locator(".dock-item").first.click()

        # Wait for launcher to be active
        print("Waiting for launcher...")
        page.wait_for_selector(".launcher.active")

        # Type a query
        print("Typing search query...")
        search_input = page.locator("#launcher-search")
        search_input.fill("GIMP")

        # Verify clear button appears
        print("Verifying clear button appears...")
        clear_btn = page.locator("#search-clear")
        try:
            clear_btn.wait_for(state="visible", timeout=2000)
            print("Clear button is visible.")
            # Take screenshot of visible button
            page.screenshot(path="verification/search_with_button.png")
            print("Screenshot saved to verification/search_with_button.png")
        except Exception as e:
            print(f"Clear button NOT visible: {e}")
            page.screenshot(path="verification/failure_no_button.png")
            browser.close()
            return False

        # Click clear button
        print("Clicking clear button...")
        clear_btn.click()

        # Verify input is empty
        print("Verifying input is empty...")
        input_value = search_input.input_value()
        if input_value == "":
            print("Input is empty.")
        else:
            print(f"Input is NOT empty: '{input_value}'")
            page.screenshot(path="verification/failure_not_empty.png")
            browser.close()
            return False

        # Verify button is hidden
        print("Verifying clear button is hidden...")
        try:
            clear_btn.wait_for(state="hidden", timeout=2000)
            print("Clear button is hidden.")
            page.screenshot(path="verification/search_cleared.png")
        except Exception as e:
            print(f"Clear button is NOT hidden: {e}")
            page.screenshot(path="verification/failure_button_not_hidden.png")
            browser.close()
            return False

        browser.close()
        return True

if __name__ == "__main__":
    success = run()
    if not success:
        print("Verification failed.")
        exit(1)
    else:
        print("Verification passed!")
        exit(0)
