import os
from playwright.sync_api import sync_playwright

def verify_xss_fix():
    # Construct file URL
    cwd = os.getcwd()
    file_path = os.path.join(cwd, 'web_ui', 'index.html')
    url = f'file://{file_path}'

    print(f"Loading {url}")

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto(url)

        # Wait for app.js to load
        page.wait_for_function('typeof spawnApp === "function"')

        # Speed up splash or wait for it
        # Let's just wait for .splash-loader to disappear
        print("Waiting for boot splash to finish...")
        page.wait_for_selector('#boot-splash', state='detached', timeout=10000)

        # Inject malicious app spawn
        malicious_name = '<img src=x onerror=alert("XSS")>'
        page.evaluate(f'spawnApp(`{malicious_name}`, "linux")')

        # Check if window was created
        window_selector = '.window'
        page.wait_for_selector(window_selector)

        # Check title
        title_selector = '.window-title'
        title_text = page.locator(title_selector).text_content()

        print(f"Window Title Text: {title_text}")

        # Check innerHTML of title to ensure it's escaped
        title_html = page.locator(title_selector).inner_html()
        print(f"Window Title HTML: {title_html}")

        # Check if img tag exists
        img_count = page.locator('.window-title img').count()
        print(f"Image tags found in title: {img_count}")

        if img_count == 0 and "&lt;img" in title_html:
             print("SUCCESS: XSS prevented.")
        else:
             print("FAILURE: XSS likely succeeded or escaping failed.")

        # Take screenshot
        output_path = os.path.join(cwd, 'verification/verification.png')
        page.screenshot(path=output_path)
        print(f"Screenshot saved to {output_path}")
        browser.close()

if __name__ == "__main__":
    verify_xss_fix()
