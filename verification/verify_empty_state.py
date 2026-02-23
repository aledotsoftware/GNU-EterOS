import os
from playwright.sync_api import sync_playwright

def verify_empty_state():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        file_path = os.path.abspath("web_ui/index.html")
        page.goto(f"file://{file_path}")

        # Wait for boot splash to disappear
        print("Waiting for boot splash to disappear...")
        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
            print("Boot splash disappeared.")
        except:
             print("Boot splash did not disappear in time.")
             page.screenshot(path="verification/error_splash_timeout.png")
             browser.close()
             return

        # Open Launcher
        print("Opening launcher...")
        launcher_btn = page.get_by_label("Lanzador de aplicaciones")
        launcher_btn.click()

        # Wait for launcher to be active
        page.wait_for_selector("#launcher.active")
        print("Launcher opened.")

        # Type in search query that yields no results
        print("Typing in search query that yields no results...")
        search_input = page.get_by_label("Buscar aplicaciones")
        search_input.fill("zzzzzzzz")

        # Verify empty state text
        print("Verifying empty state text...")
        empty_state = page.locator("#launcher-empty")
        try:
            empty_state.wait_for(state="visible", timeout=2000)

            text = empty_state.inner_text()
            # Check for Spanish text
            if "Sin resultados" in text:
                print("Empty state text is correct (Spanish).")
            else:
                print(f"ERROR: Empty state text is incorrect. Found: '{text}'")
                page.screenshot(path="verification/error_text_incorrect.png")

            # Check for button
            button = empty_state.locator("button.empty-action-btn")
            if button.is_visible():
                print("Button 'Ver todas las apps' is visible.")
                if "Ver todas las apps" in button.inner_text():
                     print("Button text is correct.")
                else:
                     print(f"ERROR: Button text is incorrect: '{button.inner_text()}'")
            else:
                print("ERROR: Button 'Ver todas las apps' is NOT visible.")
                page.screenshot(path="verification/error_button_missing.png")
                # Continue anyway to check click if possible, or fail gracefully
                if not button.count():
                    browser.close()
                    return

            # Take a screenshot of the empty state
            page.screenshot(path="verification/empty_state_appearance.png")

        except Exception as e:
            print(f"ERROR: Empty state verification failed: {e}")
            page.screenshot(path="verification/error_empty_state_failed.png")
            browser.close()
            return

        # Click the button
        print("Clicking 'Ver todas las apps' button...")
        try:
            button.click()
        except:
            print("Could not click button.")
            browser.close()
            return

        # Verify input is empty and results are shown
        input_value = search_input.input_value()
        if input_value == "":
            print("Input is cleared.")
        else:
            print(f"ERROR: Input is not cleared. Value: '{input_value}'")

        # Verify grid is visible
        grid = page.locator("#launcher-grid")
        if grid.is_visible():
             print("Launcher grid is visible.")
        else:
             print("ERROR: Launcher grid is NOT visible.")

        page.screenshot(path="verification/empty_state_verified.png")
        browser.close()

if __name__ == "__main__":
    verify_empty_state()
