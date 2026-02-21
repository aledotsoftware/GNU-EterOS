from playwright.sync_api import sync_playwright
import os

def run():
    print("Starting verification script: verify_search_clear.py")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        file_path = f"file://{os.getcwd()}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Open Launcher
        print("Opening Launcher...")
        page.locator('[aria-label="Lanzador de aplicaciones"]').click()
        page.wait_for_selector("#launcher.active")

        search_input = page.locator("#launcher-search")

        # Verify clear button is initially hidden (or not present yet)
        clear_btn = page.locator("#search-clear")
        if clear_btn.count() > 0:
            if clear_btn.is_visible():
                print("FAIL: Clear button should be hidden initially.")
                exit(1)
            else:
                print("PASS: Clear button is hidden initially.")
        else:
             print("INFO: Clear button not found (expected before implementation).")

        # Type text
        print("Typing 'test'...")
        search_input.fill("test")

        # Wait for button to be visible
        try:
            clear_btn.wait_for(state="visible", timeout=2000)
            print("PASS: Clear button became visible.")
            # Take screenshot
            page.screenshot(path="verification/search_with_clear_button.png")
            print("Screenshot saved to verification/search_with_clear_button.png")
        except:
            print("FAIL: Clear button did not become visible.")
            # Don't exit yet, maybe implementation is missing

        if clear_btn.count() > 0 and clear_btn.is_visible():
            # Click clear button
            print("Clicking clear button...")
            clear_btn.click()

            # Verify input is empty
            val = search_input.input_value()
            if val == "":
                print("PASS: Input cleared.")
            else:
                print(f"FAIL: Input not cleared. Value: '{val}'")

            # Verify button hidden
            if not clear_btn.is_visible():
                print("PASS: Clear button hidden after click.")
            else:
                print("FAIL: Clear button still visible after click.")

            # Verify focus
            is_focused = page.evaluate('document.activeElement.id === "launcher-search"')
            if is_focused:
                print("PASS: Search input has focus.")
            else:
                print("FAIL: Search input lost focus.")

        browser.close()

if __name__ == "__main__":
    run()
