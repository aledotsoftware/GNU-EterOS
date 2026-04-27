import os
import time
from playwright.sync_api import sync_playwright

def benchmark_search():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        file_path = "file:///app/web_ui/index.html"
        page.goto("file:///app/web_ui/index.html")

        try:
            page.wait_for_selector("#boot-splash", state="detached", timeout=10000)
        except:
            print("Boot splash timeout")
            return

        page.locator("#launcher-trigger").click()
        page.wait_for_selector("#launcher.active")

        # Inject benchmark code
        benchmark_script = """
        () => {
            const input = document.getElementById('launcher-search');
            const start = performance.now();
            const iterations = 1000;

            for (let i = 0; i < iterations; i++) {
                // Alternating queries to force filtering changes
                input.value = (i % 2 === 0) ? 'linux' : 'android';
                filterApps();
            }

            const end = performance.now();
            return (end - start) / iterations;
        }
        """

        avg_time = page.evaluate(benchmark_script)
        print(f"Average time per filterApps() call: {avg_time:.4f} ms")

        browser.close()

if __name__ == "__main__":
    benchmark_search()
