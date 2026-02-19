from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Navigating to: {file_path}")

        page.goto(file_path)

        # Click the Launcher button in the dock
        page.get_by_label("Lanzador de aplicaciones").click()

        # Wait for launcher to appear
        page.wait_for_selector("#launcher.active")

        # The search input is auto-focused by the app.js logic
        # Press Tab to move to the first app (GIMP)
        page.keyboard.press("Tab")

        # Get focused element info
        focused_label = page.evaluate("document.activeElement.getAttribute('aria-label')")
        print(f"Focused element label: {focused_label}")

        # Take a screenshot
        page.screenshot(path="verification/launcher_focus.png")

        browser.close()

if __name__ == "__main__":
    run()
