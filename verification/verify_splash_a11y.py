
from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Get absolute path to index.html
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"

        print(f"Navigating to {file_path}")
        page.goto(file_path)

        # 1. Verify SVG accessibility
        svg_locator = page.locator("svg.splash-logo")
        if svg_locator.count() > 0:
            role = svg_locator.get_attribute("role")
            label = svg_locator.get_attribute("aria-label")
            print(f"SVG Role: {role}")
            print(f"SVG Label: {label}")

            if role == "img" and label == "Eter OS":
                print("PASS: SVG has correct a11y attributes.")
            else:
                print("FAIL: SVG missing correct role or aria-label.")
                exit(1)
        else:
            print("FAIL: SVG logo not found!")
            exit(1)

        # 2. Verify Title Removal
        title_locator = page.locator("h1.splash-title")
        if title_locator.count() == 0:
            print("PASS: Splash title text removed (clean UX).")
        else:
            print("FAIL: Splash title text is still present!")
            exit(1)

        # 3. Verify Theme Color Meta
        meta_locator = page.locator('meta[name="theme-color"]')
        if meta_locator.count() > 0:
            content = meta_locator.get_attribute("content")
            if content == "#ffffff":
                 print("PASS: Theme color meta tag present and correct.")
            else:
                 print(f"FAIL: Theme color content is {content}, expected #ffffff")
                 exit(1)
        else:
             print("FAIL: Theme color meta tag missing!")
             exit(1)

        browser.close()

if __name__ == "__main__":
    run()
