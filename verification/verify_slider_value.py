import os
import time
from playwright.sync_api import sync_playwright

def verify_slider_value():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # Wait for boot splash to disappear
        print("Waiting for boot splash to disappear...")
        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
            print("Boot splash disappeared.")
        except:
             print("Boot splash did not disappear in time.")
             page.screenshot(path="verification/error_splash_timeout.png")
             browser.close()
             return

        # Open Control Center
        print("Opening Control Center...")
        cc_trigger = page.locator("#cc-trigger")
        cc_trigger.click()

        # Wait for Control Center to be active
        page.wait_for_selector("#control-center.active")
        print("Control Center opened.")

        # Verify initial values
        # We look for the spans with class .slider-value
        slider_values = page.locator(".slider-value")
        count = slider_values.count()
        print(f"Found {count} slider values.")

        if count != 2:
            print("ERROR: Expected 2 slider values.")
            page.screenshot(path="verification/error_slider_count.png")
            browser.close()
            return

        val1 = slider_values.nth(0).inner_text()
        val2 = slider_values.nth(1).inner_text()
        print(f"Initial values: {val1}, {val2}")

        if val1 != "80%" or val2 != "60%":
             print("ERROR: Initial values incorrect.")

        # Change slider value
        print("Changing slider values...")
        sliders = page.locator(".cc-slider")

        # Brightness
        # Dispatch input event manually as fill might not trigger it in the same way for range?
        # Playwright's fill on range input should work, but let's be safe.
        # Actually, for range input, we might need to use evaluate
        sliders.nth(0).evaluate("el => { el.value = 50; el.dispatchEvent(new Event('input')); }")

        # Volume
        sliders.nth(1).evaluate("el => { el.value = 25; el.dispatchEvent(new Event('input')); }")

        # Check new values
        new_val1 = slider_values.nth(0).inner_text()
        new_val2 = slider_values.nth(1).inner_text()
        print(f"New values: {new_val1}, {new_val2}")

        if new_val1 == "50%" and new_val2 == "25%":
            print("SUCCESS: Slider values updated correctly.")
        else:
            print("ERROR: Slider values did not update correctly.")

        # Take screenshot
        page.screenshot(path="verification/slider_verification.png")

        browser.close()

if __name__ == "__main__":
    verify_slider_value()
