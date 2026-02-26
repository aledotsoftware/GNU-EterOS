const { spawnApp } = require('./app');

describe('App Switcher Security', () => {
    let app;

    beforeEach(() => {
        // Setup DOM
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="active"></div>
            <div id="launcher-trigger"></div>
            <div id="switcher"></div>
            <div id="switcher-list"></div>
        `;

        // Mock requestAnimationFrame
        global.requestAnimationFrame = (cb) => cb();

        jest.resetModules();
        app = require('./app');
    });

    test('should prevent XSS in App Switcher (regression test)', () => {
        const maliciousName = '<img src=x onerror=alert(1)>';

        app.spawnApp(maliciousName, 'native');

        // Trigger Alt+Q
        const event = new KeyboardEvent('keydown', {
            key: 'q',
            altKey: true,
            bubbles: true
        });
        document.dispatchEvent(event);

        const switcherList = document.getElementById('switcher-list');
        // console.log('Switcher List HTML:', switcherList.innerHTML);

        const img = switcherList.querySelector('div > span > img');

        if (img) {
             console.log('VULNERABLE: Found injected image tag!');
        } else {
             console.log('SECURE: No injected image tag found.');
        }

        // This test should pass if the fix is implemented correctly (img is null)
        expect(img).toBeNull();
    });
});
