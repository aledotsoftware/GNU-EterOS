import os
import re
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

        # Wait for boot splash to disappear
        print("Waiting for boot splash...")
        page.locator("#boot-splash").wait_for(state="detached", timeout=10000)

        # Open Control Center
        print("Opening Control Center...")
        page.locator("#cc-trigger").click()

        # Wait for Control Center to be visible/active
        cc = page.locator("#control-center")
        # The class 'active' is added.
        expect(cc).to_have_class(re.compile(r"active"))

        # Verify Date
        print("Verifying date...")
        date_el = page.locator("#cc-date")
        expect(date_el).not_to_be_empty()
        print(f"Date text: {date_el.text_content()}")

        # Verify Slider Values
        print("Verifying slider values...")
        values = page.locator(".slider-value")

        # Check initial values
        expect(values.nth(0)).to_have_text("80%")
        expect(values.nth(1)).to_have_text("60%")

        # Change slider value
        print("Changing slider value...")
        slider1 = page.locator(".cc-slider").nth(0)
        # Using evaluate to simulate user interaction on range input
        slider1.evaluate("el => { el.value = 50; el.dispatchEvent(new Event('input')); }")

        # Check updated value
        expect(values.nth(0)).to_have_text("50%")

        # Take screenshot
        print("Taking screenshot...")
        if not os.path.exists("verification"):
            os.makedirs("verification")
        page.screenshot(path="verification/ux_verification.png")

        browser.close()
        print("Verification complete.")

if __name__ == "__main__":
    run()
