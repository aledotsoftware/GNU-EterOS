import os
import time
from playwright.sync_api import sync_playwright

def verify_clock(page):
    # Load index.html
    cwd = os.getcwd()
    page.goto(f"file://{cwd}/web_ui/index.html")

    # Wait for boot splash to disappear
    # The boot splash has a 2.5s delay then 0.6s fade out.
    # We can wait for the selector to be detached or hidden.
    page.wait_for_selector("#boot-splash", state="detached", timeout=10000)

    # Check if clock is visible and has text
    clock = page.locator("#clock")
    if clock.is_visible():
        print(f"Clock is visible: {clock.text_content()}")
    else:
        print("Clock is NOT visible")

    # Check cc-trigger aria-label
    trigger = page.locator("#cc-trigger")
    aria_label = trigger.get_attribute("aria-label")
    print(f"Trigger aria-label: {aria_label}")

    # Take screenshot
    page.screenshot(path="verification/clock_verification.png")

if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        try:
            verify_clock(page)
        except Exception as e:
            print(f"Error: {e}")
        finally:
            browser.close()
