import os
from playwright.sync_api import sync_playwright, expect

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # 1. Verify Icons are Focusable (tabindex)
        print("Checking if icons are focusable...")
        icons = page.locator(".icon")

        # We expect at least one icon
        expect(icons.first).to_be_visible()

        # Check attributes
        count = icons.count()
        for i in range(count):
            icon = icons.nth(i)
            # Expect tabindex="0"
            expect(icon).to_have_attribute("tabindex", "0")
            # Expect role="button" (optional but good practice)
            expect(icon).to_have_attribute("role", "button")

        # 2. Verify Keyboard Activation
        print("Testing keyboard activation (Enter key)...")

        # Focus the first icon
        icons.first.focus()

        # Press Enter
        page.keyboard.press("Enter")

        # Check if window opened (Calculator)
        window_title = page.locator(".window .title-bar span", has_text="Calculator")
        expect(window_title).to_be_visible()

        # 3. Verify Close Button Accessibility
        print("Checking close button accessibility...")
        close_btn = page.locator(".window-controls button")
        expect(close_btn).to_have_attribute("aria-label", "Close window")

        # Close the window
        close_btn.click()
        expect(window_title).not_to_be_visible()

        browser.close()
        print("Accessibility verification passed!")

if __name__ == "__main__":
    run()
