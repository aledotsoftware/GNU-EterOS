import os
from playwright.sync_api import sync_playwright

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        cwd = os.getcwd()
        page.goto(f"file://{cwd}/web_ui/index.html")

        # Wait for boot splash to disappear (it takes ~2.5s + fade out)
        print("Waiting for boot splash...")
        page.wait_for_selector("#boot-splash", state="detached", timeout=10000)

        # Open launcher
        print("Opening launcher...")
        page.click("#launcher-trigger")
        page.wait_for_selector("#launcher", state="visible")

        # Type search query
        print("Typing search query 'linux'...")
        page.fill("#launcher-search", "linux")

        # Allow a brief moment for DOM updates (though sync, RAF might delay rendering slightly)
        page.wait_for_timeout(100)

        # Take screenshot
        page.screenshot(path="verification/search_linux.png")

        # Verify items are filtered
        # Check that 'GIMP' (Linux) is visible and 'Spotify' (Android) is hidden
        # Note: In the original code, style.display is set to 'flex' or 'none'.
        # However, checking computed style is safer.

        gimp_visible = page.is_visible("div.launcher-item:has-text('GIMP')")
        spotify_visible = page.is_visible("div.launcher-item:has-text('Spotify')")

        print(f"GIMP visible: {gimp_visible}")
        print(f"Spotify visible: {spotify_visible}")

        if not gimp_visible:
            print("FAILURE: GIMP should be visible")
            exit(1)

        if spotify_visible:
            print("FAILURE: Spotify should be hidden")
            exit(1)

        print("SUCCESS: Search filtering works correctly.")

        browser.close()

if __name__ == "__main__":
    run()
