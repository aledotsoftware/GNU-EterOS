from playwright.sync_api import sync_playwright
import os

def verify_app(page):
    page.goto(f"file://{os.path.abspath(os.path.join(os.path.dirname(__file__), '../web_ui/index.html'))}")
    # Take a screenshot
    page.screenshot(path="verification/app_loaded.png")

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page()
    verify_app(page)
    browser.close()
