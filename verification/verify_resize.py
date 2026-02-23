from playwright.sync_api import sync_playwright
import os
import time

def run():
    print("Starting resize verification script...")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        file_path = f"file://{os.getcwd()}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Wait for boot splash to be gone (detached)
        print("Waiting for boot splash to finish...")
        page.locator("#boot-splash").wait_for(state="detached", timeout=10000)

        # Spawn an app window first.
        # Click the "Eter" menu trigger (.os-logo)
        print("Spawning About window...")
        page.evaluate("spawnAbout()")

        # Wait for window to appear
        window = page.locator(".window").first
        window.wait_for(state="visible")

        # Get initial dimensions
        initial_box = window.bounding_box()
        print(f"Initial size: {initial_box['width']}x{initial_box['height']}")

        # Find the right resizer (right edge)
        resizer = window.locator(".resizer-r")
        resizer.wait_for(state="visible")

        # Perform drag
        resizer_box = resizer.bounding_box()
        center_x = resizer_box['x'] + resizer_box['width'] / 2
        center_y = resizer_box['y'] + resizer_box['height'] / 2

        print("Dragging resizer...")
        page.mouse.move(center_x, center_y)
        page.mouse.down()
        # Drag 100px to the right
        page.mouse.move(center_x + 100, center_y, steps=10) # steps make it smoother events
        page.mouse.up()

        # Wait a bit for any animation (though resize should be instant/per frame)
        time.sleep(0.5)

        # Get new dimensions
        final_box = window.bounding_box()
        print(f"Final size: {final_box['width']}x{final_box['height']}")

        if final_box['width'] >= initial_box['width'] + 90: # Allow some tolerance
            print("Resize successful!")
        else:
            print("Resize failed!")
            exit(1)

        browser.close()

if __name__ == "__main__":
    run()
