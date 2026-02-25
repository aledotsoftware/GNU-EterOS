const { spawnApp } = require('./app');

describe('Window Controls', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="launcher"></div>
        `;
    });

    test('spawnApp should add data-tooltip to window controls and remove title', () => {
        spawnApp('Test App', 'native');
        const win = document.querySelector('.window');

        const minimize = win.querySelector('.minimize');
        const maximize = win.querySelector('.maximize');
        const close = win.querySelector('.close');
        const focus = win.querySelector('.focus');

        // Verify data-tooltip is set
        expect(minimize.getAttribute('data-tooltip')).toBe('Minimizar');
        expect(maximize.getAttribute('data-tooltip')).toBe('Maximizar');
        expect(close.getAttribute('data-tooltip')).toBe('Cerrar');
        expect(focus.getAttribute('data-tooltip')).toBe('Modo Focus');

        // Verify title is NOT set (to avoid double tooltips)
        expect(minimize.getAttribute('title')).toBeNull();
        expect(maximize.getAttribute('title')).toBeNull();
        expect(close.getAttribute('title')).toBeNull();
        expect(focus.getAttribute('title')).toBeNull();

        // Verify ARIA labels are preserved
        expect(minimize.getAttribute('aria-label')).toBe('Minimizar ventana');
        expect(maximize.getAttribute('aria-label')).toBe('Maximizar ventana');
        expect(close.getAttribute('aria-label')).toBe('Cerrar ventana');
        expect(focus.getAttribute('aria-label')).toBe('Cambiar a modo enfoque');
    });
});
