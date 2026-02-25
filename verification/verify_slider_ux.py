from playwright.sync_api import sync_playwright, expect
import os
import time

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading: {file_path}")
        page.goto(file_path)

        # Wait for boot splash to disappear
        print("Waiting for boot splash...")
        page.wait_for_timeout(3000)

        # Open Control Center
        print("Opening Control Center...")
        cc_trigger = page.locator("#cc-trigger")
        cc_trigger.click()

        # Wait for animation
        page.wait_for_timeout(1000)

        slider = page.locator(".cc-slider").nth(0)
        icon = slider.locator("xpath=preceding-sibling::img")

        # Initial check
        print("Checking initial state...")
        expect(slider).to_have_attribute("aria-valuetext", "80%")
        # Wait a bit for initial render/script
        page.wait_for_timeout(500)

        opacity_style = icon.evaluate("el => el.style.opacity")
        print(f"Initial Style Opacity: {opacity_style}")
        # The script sets it on init: setupSliders() -> update() -> sets opacity

        # Change value
        print("Changing value to 20%...")
        slider.evaluate("el => { el.value = '20'; el.dispatchEvent(new Event('input')); }")

        # Check ARIA
        expect(slider).to_have_attribute("aria-valuetext", "20%")

        # Check style opacity immediately (logic verification)
        opacity_style = float(icon.evaluate("el => el.style.opacity"))
        print(f"New Style Opacity: {opacity_style}")
        assert 0.43 < opacity_style < 0.45, f"Expected opacity ~0.44, got {opacity_style}"

        # Wait for transition (200ms) + buffer
        print("Waiting for transition...")
        page.wait_for_timeout(500)

        # Check computed opacity (visual verification)
        opacity_computed = float(icon.evaluate("el => getComputedStyle(el).opacity"))
        print(f"New Computed Opacity: {opacity_computed}")
        assert 0.43 < opacity_computed < 0.45, f"Expected computed opacity ~0.44, got {opacity_computed}"

        # Take screenshot
        page.locator("#control-center").screenshot(path="verification/slider_ux.png")
        print("Screenshot saved")

        browser.close()

if __name__ == "__main__":
    run()
