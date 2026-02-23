import os
from playwright.sync_api import sync_playwright

def verify_search_optimization_screenshot():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
        except:
            print("Boot splash timeout")
            return

        page.get_by_label("Lanzador de aplicaciones").click()
        page.wait_for_selector("#launcher.active")

        # Type GIMP
        search_input = page.get_by_label("Buscar aplicaciones")
        search_input.fill("GIMP")

        # Verify Firefox is hidden
        firefox_item = page.locator(".launcher-item", has_text="Firefox").first

        # Check visibility
        is_visible = firefox_item.is_visible()
        print(f"Firefox visible after typing GIMP: {is_visible}")

        # Check innerText of the span inside Firefox item
        inner_text = page.evaluate("""
            () => {
                const items = Array.from(document.querySelectorAll('.launcher-item'));
                const firefox = items.find(i => i.querySelector('span').textContent.includes('Firefox'));
                return firefox ? firefox.querySelector('span').innerText : 'NOT FOUND';
            }
        """)
        print(f"Firefox innerText while hidden: '{inner_text}'")

        # Set to Fire
        page.evaluate("""
            const input = document.getElementById('launcher-search');
            input.value = 'Fire';
            input.dispatchEvent(new Event('input', { bubbles: true }));
        """)

        if not firefox_item.is_visible():
             print("FAIL: Firefox is NOT visible after changing search to 'Fire'")
        else:
             print("PASS: Firefox is visible.")
             page.screenshot(path="verification/frontend_verification.png")

        browser.close()

if __name__ == "__main__":
    verify_search_optimization_screenshot()
