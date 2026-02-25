const { setupSliders } = require('./app');

describe('setupSliders', () => {
    let slider;
    let valueDisplay;

    beforeEach(() => {
        document.body.innerHTML = `
            <div class="cc-slider-group">
                <input type="range" class="cc-slider" value="50">
                <span class="slider-value">50%</span>
            </div>
        `;
        slider = document.querySelector('.cc-slider');
        valueDisplay = document.querySelector('.slider-value');
    });

    test('should update value display on input', () => {
        setupSliders();

        // Change value
        slider.value = "75";
        slider.dispatchEvent(new Event('input'));

        expect(valueDisplay.textContent).toBe('75%');
    });

    test('should sync initial value', () => {
        slider.value = "25";
        // HTML has 50%, but setupSliders should sync it to 25%

        setupSliders();
        expect(valueDisplay.textContent).toBe('25%');
    });
});
