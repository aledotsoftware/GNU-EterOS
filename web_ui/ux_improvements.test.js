const { updateClock, setupSliders } = require('./app');

describe('UX Improvements', () => {
    beforeEach(() => {
        // Mock DOM
        document.body.innerHTML = `
            <div id="cc-trigger" role="button" aria-label="Abrir centro de control">
                <span class="net">WiFi 6</span>
                <span class="battery">100%</span>
                <span id="clock"></span>
            </div>
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
        const mockDate = new Date(2023, 1, 17, 12, 0); // Feb 17, 2023 12:00
        jest.useFakeTimers().setSystemTime(mockDate);

        updateClock();

        const dateEl = document.getElementById('cc-date');
        // Check for Spanish output "17 feb" or English if not supported.
        expect(dateEl.innerText).toMatch(/17|feb/i);
    });

    test('updateClock should add title to clock and update aria-label of trigger', () => {
        const mockDate = new Date(2023, 9, 12, 10, 30); // Oct 12, 2023 10:30
        jest.useFakeTimers().setSystemTime(mockDate);

        updateClock();

        const clock = document.getElementById('clock');
        const trigger = document.getElementById('cc-trigger');

        // Verify clock time
        const timeString = mockDate.toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit' });
        expect(clock.textContent).toBe(timeString);

        // Verify clock title (date tooltip)
        const dateString = mockDate.toLocaleDateString('es-ES', { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
        expect(clock.title).toBe(dateString);

        // Verify trigger aria-label
        const label = trigger.getAttribute('aria-label');
        expect(label).toContain(timeString);
        expect(label).toContain(dateString);
        expect(label).toContain('WiFi 6');
        expect(label).toContain('100%');
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

    test('setupSliders should update aria-valuetext and icon opacity', () => {
        // Mock structure with icon
        document.body.innerHTML = `
            <div class="cc-slider-group">
                <img class="slider-icon" src="icon.png" style="opacity: 1">
                <input type="range" class="cc-slider" value="50" aria-label="Volume">
                <span class="slider-value"></span>
            </div>
        `;

        setupSliders();

        const slider = document.querySelector('.cc-slider');
        const icon = document.querySelector('.slider-icon');

        // Check initial state
        expect(slider.getAttribute('aria-valuetext')).toBe('50%');
        expect(slider.getAttribute('aria-valuenow')).toBe('50');
        // Opacity check: 0.3 + (50/100 * 0.7) = 0.65
        expect(parseFloat(icon.style.opacity)).toBeCloseTo(0.65);

        // Update value
        slider.value = '100';
        slider.dispatchEvent(new Event('input'));

        expect(slider.getAttribute('aria-valuetext')).toBe('100%');
        expect(parseFloat(icon.style.opacity)).toBe(1);

        slider.value = '0';
        slider.dispatchEvent(new Event('input'));
        expect(parseFloat(icon.style.opacity)).toBeCloseTo(0.3);
    });
});
