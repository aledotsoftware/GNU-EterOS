const { spawnApp } = require('./app');

describe('UX Feedback - Window Focus', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher"></div>
        `;
        jest.useFakeTimers();
    });

    afterEach(() => {
        jest.useRealTimers();
    });

    test('should add shake animation when spawning existing app', () => {
        // First spawn
        spawnApp('Test App', 'native');
        const win = document.querySelector('.window');
        expect(win).toBeTruthy();
        expect(win.classList.contains('shake')).toBe(false);

        // Second spawn (should trigger shake)
        spawnApp('Test App', 'native');

        // Window should still be there (only 1)
        const wins = document.querySelectorAll('.window');
        expect(wins.length).toBe(1);

        // Should have shake class
        expect(win.classList.contains('shake')).toBe(true);

        // Should remove shake class after animation
        jest.advanceTimersByTime(500);
        expect(win.classList.contains('shake')).toBe(false);
    });
});
