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

        # Type in search a query with no results
        print("Typing in search query with no results ('zzzz')...")
        search_input = page.get_by_label("Buscar aplicaciones")
        search_input.fill("zzzz")

        # Verify empty state text
        empty_state = page.locator("#launcher-empty")
        try:
            empty_state.wait_for(state="visible", timeout=2000)
            print("Empty state is visible.")
        except:
            print("ERROR: Empty state is NOT visible.")
            page.screenshot(path="verification/error_not_visible.png")
            browser.close()
            return

        text = empty_state.locator("h3").inner_text()
        if text == "Sin resultados":
            print("Empty state text is correct ('Sin resultados').")
        else:
            print(f"ERROR: Empty state text is incorrect. Expected 'Sin resultados', got '{text}'")

        # Verify button presence
        clear_action_btn = empty_state.locator("button.empty-action-btn")
        if clear_action_btn.is_visible():
             print("Action button is visible.")
             if clear_action_btn.inner_text() == "Ver todas las apps":
                 print("Action button text is correct.")
             else:
                 print(f"Action button text is incorrect. Got '{clear_action_btn.inner_text()}'")
        else:
             print("Action button is NOT visible (Expected if not implemented yet).")

        # Take screenshot of the empty state
        page.screenshot(path="verification/empty_state_visible.png")

        # Click the button if visible (simulating user action)
        if clear_action_btn.is_visible():
            print("Clicking action button...")
            clear_action_btn.click()

            # Verify input is cleared
            input_value = search_input.input_value()
            if input_value == "":
                print("Input is empty after clicking action button.")
            else:
                print(f"ERROR: Input is not empty. Value: '{input_value}'")

            # Verify results are back (grid is visible)
            grid = page.locator("#launcher-grid")
            if grid.is_visible():
                print("Launcher grid is visible again.")
            else:
                print("ERROR: Launcher grid is NOT visible.")

        page.screenshot(path="verification/empty_state_cleared.png")
        browser.close()

if __name__ == "__main__":
    verify_empty_state()
