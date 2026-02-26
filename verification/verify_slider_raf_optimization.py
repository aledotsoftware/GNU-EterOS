from playwright.sync_api import sync_playwright, expect
import os

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
        value_display = slider.locator("xpath=following-sibling::span")
        icon = slider.locator("xpath=preceding-sibling::img")

        # Initial check
        print("Checking initial state...")
        expect(slider).to_have_attribute("aria-valuetext", "80%")
        expect(value_display).to_have_text("80%")

        # Verify initial opacity
        opacity_style = float(icon.evaluate("el => el.style.opacity"))
        print(f"Initial Opacity: {opacity_style}")
        assert 0.85 < opacity_style < 0.87, f"Expected opacity ~0.86, got {opacity_style}"

        # Change value multiple times quickly (simulating drag)
        print("Changing value rapidly...")
        for val in ["70", "60", "50", "40", "30", "20"]:
            slider.evaluate(f"el => {{ el.value = '{val}'; el.dispatchEvent(new Event('input')); }}")
            # Minimal wait to simulate fast input
            page.wait_for_timeout(10)

        # Check final state (20%)
        print("Checking final state (20%)...")
        expect(slider).to_have_attribute("aria-valuetext", "20%")
        expect(value_display).to_have_text("20%")

        # Check final opacity
        # 0.3 + (20/100) * 0.7 = 0.3 + 0.14 = 0.44
        opacity_style = float(icon.evaluate("el => el.style.opacity"))
        print(f"Final Opacity: {opacity_style}")
        assert 0.43 < opacity_style < 0.45, f"Expected opacity ~0.44, got {opacity_style}"

        # Take screenshot for visual verification
        page.screenshot(path="verification/slider_raf_optimization.png")
        print("Screenshot saved to verification/slider_raf_optimization.png")

        print("Verification passed!")
        browser.close()

if __name__ == "__main__":
    run()
