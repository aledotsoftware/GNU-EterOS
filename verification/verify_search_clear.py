import os
from playwright.sync_api import sync_playwright

def verify_search_clear():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

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

        # Verify clear button is visible
        clear_btn = page.get_by_label("Limpiar búsqueda")
        if clear_btn.is_visible():
            print("Clear button is visible.")
        else:
            print("ERROR: Clear button is NOT visible.")
            page.screenshot(path="verification/error_not_visible.png")
            browser.close()
            return

        # Take screenshot of visible button
        page.screenshot(path="verification/search_with_text.png")

        # Click clear button
        print("Clicking clear button...")
        clear_btn.click()

        # Verify input is empty
        input_value = search_input.input_value()
        if input_value == "":
            print("Input is empty.")
        else:
            print(f"ERROR: Input is not empty. Value: '{input_value}'")

        # Verify clear button is hidden
        if not clear_btn.is_visible():
             print("Clear button is hidden.")
        else:
             print("ERROR: Clear button is still visible.")

        # Take final screenshot
        page.screenshot(path="verification/search_cleared.png")

        browser.close()

if __name__ == "__main__":
    verify_search_clear()
