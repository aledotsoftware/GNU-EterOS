const { toggleEterMenu, setupSliders } = require('./app');

describe('toggleEterMenu', () => {
    let menu;
    let trigger;
    let controlCenter;
    let ccTrigger;
    let launcher;
    let launcherTrigger;

    beforeEach(() => {
        // Set up DOM
        document.body.innerHTML = `
            <div id="eter-menu" class="eter-menu">
                <div class="menu-item" tabindex="0">Item 1</div>
            </div>
            <div class="os-logo" tabindex="0"></div>
            <div id="control-center" class="control-center"></div>
            <div id="cc-trigger"></div>
            <div id="launcher" class="launcher"></div>
            <div id="launcher-trigger"></div>
        `;

        menu = document.getElementById('eter-menu');
        trigger = document.querySelector('.os-logo');
        controlCenter = document.getElementById('control-center');
        ccTrigger = document.getElementById('cc-trigger');
        launcher = document.getElementById('launcher');
        launcherTrigger = document.getElementById('launcher-trigger');
    });

    test('should toggle active class on menu', () => {
        toggleEterMenu();
        expect(menu.classList.contains('active')).toBe(true);
        toggleEterMenu();
        expect(menu.classList.contains('active')).toBe(false);
    });

    test('should update aria-expanded on trigger', () => {
        toggleEterMenu();
        expect(trigger.getAttribute('aria-expanded')).toBe('true');
        toggleEterMenu();
        expect(trigger.getAttribute('aria-expanded')).toBe('false');
    });

    test('should stop propagation if event is provided', () => {
        const mockEvent = { stopPropagation: jest.fn() };
        toggleEterMenu(mockEvent);
        expect(mockEvent.stopPropagation).toHaveBeenCalled();
    });

    test('should close control center and launcher when opening menu', () => {
        // Open CC and Launcher
        controlCenter.classList.add('active');
        launcher.classList.add('active');

        toggleEterMenu();

        expect(controlCenter.classList.contains('active')).toBe(false);
        expect(launcher.classList.contains('active')).toBe(false);
        expect(ccTrigger.getAttribute('aria-expanded')).toBe('false');
        expect(launcherTrigger.getAttribute('aria-expanded')).toBe('false');
    });

    test('should focus first menu item when opening', () => {
        const firstItem = menu.querySelector('.menu-item');
        const spy = jest.spyOn(firstItem, 'focus');
        toggleEterMenu();
        expect(spy).toHaveBeenCalled();
    });

    test('should focus trigger when closing', () => {
        menu.classList.add('active');
        // We need to simulate the initial state where menu is active
        trigger.setAttribute('aria-expanded', 'true');

        const spy = jest.spyOn(trigger, 'focus');
        toggleEterMenu();
        expect(spy).toHaveBeenCalled();
    });
});

describe('setupSliders', () => {
    let slider;
    let span;

    beforeEach(() => {
        // Mock requestAnimationFrame to execute immediately for testing throttled updates
        jest.spyOn(window, 'requestAnimationFrame').mockImplementation(cb => cb());

        document.body.innerHTML = `
            <div class="cc-slider-group">
                <input type="range" class="cc-slider" value="50">
                <span class="slider-value">50%</span>
            </div>
        `;
        slider = document.querySelector('.cc-slider');
        span = document.querySelector('.slider-value');
        setupSliders();
    });

    afterEach(() => {
        jest.restoreAllMocks();
    });

    test('should update span text when slider value changes', () => {
        slider.value = '75';
        slider.dispatchEvent(new Event('input'));
        expect(span.textContent).toBe('75%');
    });

    test('should not throw error if span is missing', () => {
        document.body.innerHTML = `<input type="range" class="cc-slider" value="50">`;
        setupSliders();
        const input = document.querySelector('input');
        input.value = '75';
        expect(() => input.dispatchEvent(new Event('input'))).not.toThrow();
    });
});
