const { spawnApp, maximizeWindow } = require('./app');

describe('spawnApp Security', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="active"></div>
            <div id="launcher-trigger"></div>
            <div id="switcher">
                <div id="switcher-list"></div>
            </div>
            <div class="os-shell"></div>
            <span class="active-app-name"></span>
            <div id="control-center"></div>
            <div id="eter-menu"></div>
        `;
        // Mock requestAnimationFrame for drag logic inside spawnApp
        global.requestAnimationFrame = (cb) => cb();
    });

    test('should prevent XSS in window title', () => {
        const maliciousName = '<img src=x onerror=alert(1)>';
        spawnApp(maliciousName, 'native');

        const workspace = document.getElementById('workspace');
        const win = workspace.querySelector('.window');
        const title = win.querySelector('.window-title');

        // If vulnerable, the title element will contain an IMG element
        const img = title.querySelector('img');

        // Assert that NO image tag was created (it should be treated as text)
        expect(img).toBeNull();

        // Assert that the title TEXT contains the string, but not as HTML
        expect(title.textContent).toContain(maliciousName);
    });

    test('should prevent XSS in App Switcher', () => {
        const maliciousName = '<img src=x onerror=console.log("XSS")>';
        spawnApp(maliciousName, 'native');

        // Trigger showSwitcher via event
        const event = new window.KeyboardEvent('keydown', { key: 'q', altKey: true });
        document.dispatchEvent(event);

        const list = document.getElementById('switcher-list');
        const cardSpan = list.querySelector('span');

        // Expect escaped HTML entities in innerHTML, confirming XSS prevention
        // The malicious payload should be treated as text.
        expect(cardSpan.innerHTML).toBe('&lt;img src=x onerror=console.log("XSS")&gt; (NATIVE)');
    });

    test('should prevent XSS in Maximize Window', () => {
        const maliciousName = '<img src=x onerror=alert>';
        spawnApp(maliciousName, 'native');

        const win = document.querySelector('.window');
        const maximizeBtn = win.querySelector('.control.maximize');
        const appNameDisplay = document.querySelector('.active-app-name');

        // Call maximizeWindow to verify the fix
        maximizeWindow(maximizeBtn);

        // Expect escaped HTML in the display
        expect(appNameDisplay.innerHTML).toContain('&lt;img src=x onerror=alert&gt;');
    });
});
