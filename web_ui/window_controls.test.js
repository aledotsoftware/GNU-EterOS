const { spawnApp } = require('./app');

describe('Window Controls', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="launcher"></div>
        `;
    });

    test('spawnApp should add title tooltips to window controls', () => {
        spawnApp('Test App', 'native');
        const win = document.querySelector('.window');

        const minimize = win.querySelector('.minimize');
        const maximize = win.querySelector('.maximize');
        const close = win.querySelector('.close');

        expect(minimize.getAttribute('title')).toBe('Minimizar');
        expect(maximize.getAttribute('title')).toBe('Maximizar');
        expect(close.getAttribute('title')).toBe('Cerrar');
    });
});
