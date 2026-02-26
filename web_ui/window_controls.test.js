const { spawnApp } = require('./app');

describe('Window Controls', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="launcher"></div>
        `;
    });

    test('spawnApp should add data-tooltip tooltips and aria-labels to window controls', () => {
        spawnApp('Test App', 'native');
        const win = document.querySelector('.window');

        const focus = win.querySelector('.focus');
        const minimize = win.querySelector('.minimize');
        const maximize = win.querySelector('.maximize');
        const close = win.querySelector('.close');

        // Check custom tooltips
        expect(focus.getAttribute('data-tooltip')).toBe('Modo Focus');
        expect(minimize.getAttribute('data-tooltip')).toBe('Minimizar');
        expect(maximize.getAttribute('data-tooltip')).toBe('Maximizar');
        expect(close.getAttribute('data-tooltip')).toBe('Cerrar');

        // Check accessibility labels
        expect(focus.getAttribute('aria-label')).toBe('Cambiar a modo enfoque');
        expect(minimize.getAttribute('aria-label')).toBe('Minimizar ventana');
        expect(maximize.getAttribute('aria-label')).toBe('Maximizar ventana');
        expect(close.getAttribute('aria-label')).toBe('Cerrar ventana');
    });
});
