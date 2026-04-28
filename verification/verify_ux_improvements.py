from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local file
        cwd = os.getcwd()
        filepath = os.path.join(cwd, 'web_ui/index.html')
        page.goto(f'file://{filepath}')

        # Wait for splash screen to disappear (simulation)
        # The splash screen takes 2.5s + 0.6s fade out.
        # We can just hide it manually to speed up.

        try:
            page.evaluate("document.getElementById('boot-splash')?.remove()")
        except:
            pass

        # Spawn an app (e.g. Settings)
        # We can click the "Settings" menu item or just call spawnApp via console if exposed.
        # The 'spawnApp' function is global in app.js
        page.evaluate("spawnApp('Test App', 'native')")

        # Wait for window to appear
        page.wait_for_selector('.window')

        # Hover over the minimize button to show tooltip
        # Note: Native tooltips (title attribute) are not rendered by the browser engine in the DOM
        # in a way that screenshot captures them easily (OS dependent).
        # However, we can verify the attribute exists.

        minimize_btn = page.locator('.control.minimize')
        title = minimize_btn.get_attribute('title')
        print(f"Minimize Tooltip: {title}")

        maximize_btn = page.locator('.control.maximize')
        title_max = maximize_btn.get_attribute('title')
        print(f"Maximize Tooltip: {title_max}")

        close_btn = page.locator('.control.close')
        title_close = close_btn.get_attribute('title')
        print(f"Close Tooltip: {title_close}")

        # Capture screenshot of the window
        # We can't see the tooltip in the screenshot because it's an OS tooltip,
        # but we can see the window exists.
        page.screenshot(path='verification/ux_improvements.png')

        browser.close()

if __name__ == "__main__":
    run()
