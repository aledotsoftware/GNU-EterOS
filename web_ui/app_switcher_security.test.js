const { spawnApp } = require('./app');

describe('App Switcher Security', () => {
    beforeEach(() => {
        // Polyfill innerText for JSDOM because JSDOM implementation might be missing or different
        // and the app relies on it.
        Object.defineProperty(HTMLElement.prototype, 'innerText', {
            get() {
                return this.textContent;
            },
            set(value) {
                this.textContent = value;
            },
            configurable: true
        });

        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="active"></div>
            <div id="launcher-trigger"></div>
            <div id="switcher"></div>
            <div id="switcher-list"></div>
        `;
        global.requestAnimationFrame = (cb) => cb();
        jest.resetModules();
        require('./app');
    });

    test('should prevent XSS in App Switcher when window title contains HTML', () => {
        const maliciousName = '<img src=x onerror=alert(1)>';
        const app = require('./app');
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

        // We look for the unescaped HTML injected into the DOM
        const img = switcherList.querySelector('img[onerror]');

        // Assert that NO image tag was created
        expect(img).toBeNull();

        // Assert that the malicious string is present as TEXT, not HTML
        // Note: textContent will contain the raw string
        expect(switcherList.textContent).toContain(maliciousName);
    });
});
