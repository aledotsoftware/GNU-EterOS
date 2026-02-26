from playwright.sync_api import sync_playwright
import os

def test_sliders():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        # Load the local HTML file
        cwd = os.getcwd()
        url = f"file://{cwd}/web_ui/index.html"
        page.goto(url)

        # Open Control Center
        page.click("#cc-trigger")

        # Wait for Control Center to be visible
        # It uses class 'active'
        page.wait_for_selector("#control-center.active")

        # Find the first slider (Brightness)
        # HTML: <input type="range" class="cc-slider" value="80" aria-label="Brillo">
        slider = page.locator(".cc-slider").first

        # Verify initial value
        # The span.slider-value next to it should be 80%
        value_display = slider.locator("xpath=following-sibling::span[contains(@class, 'slider-value')]")

        # Wait for JS to initialize (it runs on DOMContentLoaded)
        page.wait_for_timeout(500)

        # Check initial text
        print(f"Initial text: {value_display.text_content()}")
        assert value_display.text_content() == "80%"

        # Change value
        slider.fill("50")
        # Dispatch input event manually as fill might not trigger it exactly like a user drag
        slider.dispatch_event("input")

        # Check updated text
        print(f"Updated text: {value_display.text_content()}")
        assert value_display.text_content() == "50%"

        # Take screenshot
        os.makedirs("verification", exist_ok=True)
        page.screenshot(path="verification/sliders_verified.png")

        browser.close()

if __name__ == "__main__":
    test_sliders()
