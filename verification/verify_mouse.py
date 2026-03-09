from playwright.sync_api import sync_playwright

def verify_app(page):
    page.goto("file:///app/web_ui/index.html")
    # Take a screenshot
    page.screenshot(path="verification/app_loaded.png")

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page()
    verify_app(page)
    browser.close()
