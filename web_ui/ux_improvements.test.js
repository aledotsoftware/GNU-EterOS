const { updateClock, setupSliders } = require('./app');

describe('UX Improvements', () => {
    beforeEach(() => {
        // Mock DOM
        document.body.innerHTML = `
            <span id="clock"></span>
            <span id="cc-date"></span>
            <div class="cc-slider-group">
                <input type="range" class="cc-slider" value="50">
                <span class="slider-value"></span>
            </div>
            <div class="cc-slider-group">
                <input type="range" class="cc-slider" value="75">
                <span class="slider-value"></span>
            </div>
        `;
    });

    test('updateClock should update the date element', () => {
        // Mock Date
        const mockDate = new Date(2023, 1, 17); // Feb 17, 2023
        jest.useFakeTimers().setSystemTime(mockDate);

        updateClock();

        const dateEl = document.getElementById('cc-date');
        // The format depends on locale, but checking it's not empty is a good start.
        // With default locale (en-US in JSDOM usually), it might be "Fri, Feb 17".
        expect(dateEl.innerText).not.toBe('');
        // We can check if it contains "Feb" or "17"
        expect(dateEl.innerText).toMatch(/Feb/);
        expect(dateEl.innerText).toMatch(/17/);
    });

    test('setupSliders should update slider values on input', () => {
        setupSliders();

        const slider = document.querySelector('.cc-slider');
        const valueDisplay = slider.nextElementSibling;

        // Initial value check
        expect(valueDisplay.textContent).toBe('50%');

        // Simulate input change
        slider.value = '60';
        slider.dispatchEvent(new Event('input'));

        expect(valueDisplay.textContent).toBe('60%');
    });

    test('setupSliders should handle multiple sliders', () => {
        setupSliders();

        const sliders = document.querySelectorAll('.cc-slider');
        const slider1 = sliders[0];
        const val1 = slider1.nextElementSibling;
        const slider2 = sliders[1];
        const val2 = slider2.nextElementSibling;

        expect(val1.textContent).toBe('50%');
        expect(val2.textContent).toBe('75%');

        slider1.value = '10';
        slider1.dispatchEvent(new Event('input'));
        expect(val1.textContent).toBe('10%');
        expect(val2.textContent).toBe('75%'); // Should not change
    });
});
