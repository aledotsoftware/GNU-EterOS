const { spawnApp } = require('./app');

describe('spawnApp Security', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="active"></div>
            <div id="launcher-trigger"></div>
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
});
