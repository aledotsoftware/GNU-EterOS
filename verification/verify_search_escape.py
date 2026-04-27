import os
from playwright.sync_api import sync_playwright

def verify_search_escape():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = "file:///app/web_ui/index.html"
        page.goto("file:///app/web_ui/index.html")

        # Wait for boot splash to disappear
        print("Waiting for boot splash to disappear...")
        page.wait_for_selector("#boot-splash", state="detached", timeout=5000)

        # Open Launcher
        print("Opening launcher...")
        launcher_btn = page.get_by_label("Lanzador de aplicaciones")
        launcher_btn.click()

        # Wait for launcher to be active
        page.wait_for_selector("#launcher.active")
        print("Launcher opened.")

        # Type in search
        print("Typing in search...")
        search_input = page.get_by_label("Buscar aplicaciones")
        search_input.fill("GIMP")

        # Verify clear button is visible (baseline check)
        clear_btn = page.get_by_label("Limpiar búsqueda")
        # Wait for visibility transition
        clear_btn.wait_for(state="visible", timeout=2000)

        if not clear_btn.is_visible():
            print("ERROR: Clear button is NOT visible after typing.")
            page.screenshot(path="verification/error_not_visible_escape.png")
            browser.close()
            return

        # Press Escape
        print("Pressing Escape...")
        search_input.press("Escape")

        # Verify input is empty
        input_value = search_input.input_value()
        if input_value == "":
            print("Input is empty.")
        else:
            print(f"ERROR: Input is not empty. Value: '{input_value}'")
            page.screenshot(path="verification/error_escape_clear_failed.png")
            browser.close()
            return

        # Verify clear button is hidden
        # Since we are using CSS transition, we might need to wait for it to become hidden
        # But wait_for(state="hidden") checks for display:none or visibility:hidden or opacity:0
        try:
            clear_btn.wait_for(state="hidden", timeout=2000)
            print("Clear button is hidden.")
        except:
             print("ERROR: Clear button is still visible.")

        # Press Escape again
        print("Pressing Escape again...")
        search_input.press("Escape")

        # Verify launcher is closed
        # Check if #launcher lacks class 'active'
        page.wait_for_function("!document.querySelector('#launcher').classList.contains('active')")
        print("Launcher closed successfully.")

        browser.close()

if __name__ == "__main__":
    verify_search_escape()
