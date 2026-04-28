
function escapeHTML(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}
function toggleEterMenu(e) {
    if (e) e.stopPropagation();
    const menu = document.getElementById('eter-menu');
    const isActive = menu.classList.toggle('active');

    // Update aria-expanded on trigger
    const trigger = document.querySelector('.os-logo');
    if (trigger) {
        trigger.setAttribute('aria-expanded', isActive);
        if (!isActive) trigger.focus();
    }

    // Hide other panels
    document.getElementById('control-center').classList.remove('active');
    const ccTrigger = document.getElementById('cc-trigger');
    if (ccTrigger) ccTrigger.setAttribute('aria-expanded', 'false');

    document.getElementById('launcher').classList.remove('active');
    const launcherTrigger = document.getElementById('launcher-trigger');
    if (launcherTrigger) launcherTrigger.setAttribute('aria-expanded', 'false');

    if (isActive) {
        const firstItem = menu.querySelector('.menu-item');
        if (firstItem) firstItem.focus();
    }
}

function handleMenuKey(e, item) {
    const menu = document.getElementById('eter-menu');
    const items = Array.from(menu.querySelectorAll('.menu-item'));
    const index = items.indexOf(item);

    if (e.key === 'ArrowDown') {
        e.preventDefault();
        const nextIndex = (index + 1) % items.length;
        items[nextIndex].focus();
    } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        const prevIndex = (index - 1 + items.length) % items.length;
        items[prevIndex].focus();
    } else if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        item.click();
    } else if (e.key === 'Escape') {
        e.preventDefault();
        toggleEterMenu();
    } else if (e.key === 'Tab') {
        // Close menu if tabbing out
        const menu = document.getElementById('eter-menu');
        if (menu.classList.contains('active')) {
            toggleEterMenu();
        }
    }
}

function spawnAbout() {
    spawnApp("Acerca de Eter OS", "native", `
        <div style="text-align: center; padding: 40px;">
            <div style="width: 80px; height: 80px; background: var(--accent-glow); border-radius: 20px; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 20px;">
                <span style="font-size: 30px; font-weight: bold;">E</span>
            </div>
            <h2>Eter OS</h2>
            <p style="opacity: 0.6;">Versión 1.0 "Convergencia"</p>
            <p style="margin-top: 20px; font-size: 14px;">El primer sistema operativo conceptual diseñado para unir la web, Android y Linux en una sola experiencia premium.</p>
        </div>
    `);
    document.getElementById('eter-menu').classList.remove('active');
}

function spawnSettings() {
    spawnApp("Configuración del Sistema", "native", `
        <div class="settings-container">
            <div class="settings-sidebar">
                <div class="menu-item active">🎨 Apariencia</div>
                <div class="menu-item">🖥️ Pantalla</div>
                <div class="menu-item">🔊 Sonido</div>
                <div class="menu-item">🌐 Red</div>
                <div class="menu-item">💾 Disco</div>
                <div class="menu-item">⚡ Multitarea</div>
                <div class="menu-item">🔒 Seguridad</div>
                <div class="menu-item">ℹ️ Acerca de</div>
            </div>
            <div class="settings-main">
                <div class="settings-group">
                    <h3>Personalización de Eter OS</h3>
                    <div class="settings-item">
                        <div>
                            <span>Tema Global</span>
                            <p style="font-size: 11px; opacity: 0.5;">Cambia el aspecto de todo el sistema.</p>
                        </div>
                        <select aria-label="Tema Global" style="background: rgba(255,255,255,0.1); color: white; border: 1px solid rgba(255,255,255,0.1); padding: 8px 12px; border-radius: 8px; cursor: pointer;">
                            <option>Oscuro Profundo (Default)</option>
                            <option>Gris Espacial</option>
                            <option>Azul Medianoche</option>
                        </select>
                    </div>
...
                    <div class="settings-item">
                        <span>Acento de Eter</span>
                        <div style="display: flex; gap: 12px;">
                            <div style="width: 28px; height: 28px; background: #38bdf8; border-radius: 50%; border: 3px solid white; box-shadow: 0 0 10px var(--accent-glow);"></div>
                            <div style="width: 28px; height: 28px; background: #a855f7; border-radius: 50%;"></div>
                            <div style="width: 28px; height: 28px; background: #22c55e; border-radius: 50%;"></div>
                            <div style="width: 28px; height: 28px; background: #f59e0b; border-radius: 50%;"></div>
                        </div>
                    </div>
                </div>
                <div class="settings-group">
                    <h3>Subsistemas de Convergencia</h3>
                    <div class="settings-item">
                        <div>
                            <span>Modo Tableta Dinámico</span>
                            <p style="font-size: 11px; opacity: 0.5;">Optimiza los controles para touch automáticamente.</p>
                        </div>
                        <input type="checkbox" checked aria-label="Modo Tableta Dinámico" style="width: 20px; height: 20px; accent-color: var(--accent);">
                    </div>
                    <div class="settings-item">
                        <span>Núcleo Linux (Eter-Core)</span>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <div style="width: 8px; height: 8px; background: #22c55e; border-radius: 50%;"></div>
                            <span style="color: #22c55e; font-size: 13px;">Ejecutando (Wayland)</span>
                        </div>
                    </div>
                    <div class="settings-item">
                        <span>Servicios Android</span>
                        <span style="color: #94a3b8; font-size: 13px;">Servicios de Google Play activos</span>
                    </div>
                </div>
                <div class="settings-group">
                    <h3>Multidispotivo</h3>
                    <div class="settings-item">
                        <span>Eter-Drop (Airdrop clone)</span>
                        <input type="checkbox" checked aria-label="Eter-Drop (Airdrop clone)">
                    </div>
                </div>
            </div>
        </div>
    `);
    document.getElementById('eter-menu').classList.remove('active');
}

function updateClock() {
    const now = new Date();
    const clock = document.getElementById('clock');
    if (clock) {
        clock.innerText = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
}

if (typeof module === 'undefined') {
    setInterval(updateClock, 1000);
    updateClock();
}

function toggleLauncher() {
    const launcher = document.getElementById('launcher');
    const isActive = launcher.classList.toggle('active');

    const trigger = document.getElementById('launcher-trigger');
    if (trigger) trigger.setAttribute('aria-expanded', isActive);

    if (isActive) {
        document.getElementById('launcher-search').focus();
    } else {
        // Clear search on close
        document.getElementById('launcher-search').value = '';
        filterApps();
    }

    // Hide CC if opening launcher
    const cc = document.getElementById('control-center');
    if (cc.classList.contains('active')) {
        cc.classList.remove('active');
        const ccTrigger = document.getElementById('cc-trigger');
        if (ccTrigger) ccTrigger.setAttribute('aria-expanded', 'false');
    }
}

function clearSearch() {
    const input = document.getElementById('launcher-search');
    input.value = '';
    input.focus();
    filterApps();
}

let launcherCache = null;

function filterApps() {
    const query = document.getElementById('launcher-search').value.toLowerCase();

    // Toggle clear button
    const clearBtn = document.getElementById('search-clear');
    if (clearBtn) {
        clearBtn.hidden = query.length === 0;
    }

    if (!launcherCache) {
        launcherCache = Array.from(document.querySelectorAll('.launcher-item')).map(item => ({
            element: item,
            name: item.querySelector('span').textContent.toLowerCase(),
            tag: item.querySelector('.tag').textContent.toLowerCase()
        }));
    }

    let hasResults = false;

    launcherCache.forEach(app => {
        if (app.name.includes(query) || app.tag.includes(query)) {
            app.element.style.display = 'flex';
            hasResults = true;
        } else {
            app.element.style.display = 'none';
        }
    });

    const emptyState = document.getElementById('launcher-empty');
    const grid = document.getElementById('launcher-grid');

    if (emptyState && grid) {
        if (hasResults) {
            emptyState.style.display = 'none';
            grid.style.display = 'grid';
        } else {
            emptyState.style.display = 'block';
            grid.style.display = 'none';
        }
    }
}

function toggleControlCenter(e) {
    if (e) e.stopPropagation();
    const cc = document.getElementById('control-center');
    const isActive = cc.classList.toggle('active');

    const trigger = document.getElementById('cc-trigger');
    if (trigger) trigger.setAttribute('aria-expanded', isActive);

    // Hide launcher if opening CC
    const launcher = document.getElementById('launcher');
    if (launcher.classList.contains('active')) {
        launcher.classList.remove('active');
        const launcherTrigger = document.getElementById('launcher-trigger');
        if (launcherTrigger) launcherTrigger.setAttribute('aria-expanded', 'false');
    }
}

// App Switcher Logic
let isSwitcherActive = false;
let switcherIndex = 0;
let openWindows = [];

document.addEventListener('keydown', (e) => {
    // Escape standard Alt+Tab by using Alt+Q (Quick Switch)
    if (e.altKey && e.key.toLowerCase() === 'q') {
        e.preventDefault();

        if (!isSwitcherActive) {
            showSwitcher();
        } else {
            cycleSwitcher();
        }
    }
});

document.addEventListener('keyup', (e) => {
    if (e.key === 'Alt') {
        if (isSwitcherActive) {
            confirmSwitcherSelection();
        }
    }
});

function showSwitcher() {
    const windows = Array.from(document.querySelectorAll('.window'));
    if (windows.length === 0) return;

    openWindows = windows;
    isSwitcherActive = true;
    switcherIndex = 0;

    const switcher = document.getElementById('switcher');
    const list = document.getElementById('switcher-list');
    list.innerHTML = '';

    openWindows.forEach((win, index) => {
        const title = win.querySelector('.window-title').innerText;
        let type = 'unknown';
        let iconSrc = '';

        if (win.classList.contains('linux-app')) {
            type = 'linux';
            iconSrc = 'https://img.icons8.com/ios-filled/50/ffffff/console.png';
        } else if (win.classList.contains('android-app')) {
            type = 'android';
            iconSrc = 'https://img.icons8.com/color/48/000000/google-play.png';
        } else if (win.classList.contains('native-app')) {
            type = 'native';
            iconSrc = 'https://img.icons8.com/ios-filled/50/ffffff/settings.png'; // Example icon for native apps
        }

        const card = document.createElement('div');
        card.className = `switcher-card ${index === 0 ? 'selected' : ''}`;
        card.innerHTML = `
            <img src="${iconSrc}" width="64">
            <span>${title}</span>
        `;
        card.onclick = () => {
            switcherIndex = index;
            confirmSwitcherSelection();
        };
        list.appendChild(card);
    });

    switcher.classList.add('active');
}

function cycleSwitcher() {
    switcherIndex = (switcherIndex + 1) % openWindows.length;
    const cards = document.querySelectorAll('.switcher-card');
    cards.forEach((card, index) => {
        card.classList.toggle('selected', index === switcherIndex);
    });
}

function confirmSwitcherSelection() {
    const selectedWin = openWindows[switcherIndex];
    if (selectedWin) {
        selectedWin.classList.remove('minimized');
        selectedWin.style.zIndex = ++zIndexCounter;
    }

    document.getElementById('switcher').classList.remove('active');
    isSwitcherActive = false;
}

// Close overlays when clicking on workspace
document.addEventListener('click', (e) => {
    if (!e.target.closest('.control-center') && !e.target.closest('.status-right')) {
        const cc = document.getElementById('control-center');
        if (cc.classList.contains('active')) {
            cc.classList.remove('active');
            const trigger = document.getElementById('cc-trigger');
            if (trigger) trigger.setAttribute('aria-expanded', 'false');
        }
    }

    const menu = document.getElementById('eter-menu');
    if (menu.classList.contains('active') && !e.target.closest('.eter-menu') && !e.target.closest('.os-logo')) {
        menu.classList.remove('active');
        const trigger = document.querySelector('.os-logo');
        if (trigger) trigger.setAttribute('aria-expanded', 'false');
    }
});

let zIndexCounter = 100;

function spawnApp(name, type, customContent = null) {
    // Check if app is already open (to restore if minimized)
    const existingWins = document.querySelectorAll('.window');
    for (let win of existingWins) {
        if (win.querySelector('.window-title').innerText.includes(name)) {
            if (win.classList.contains('minimized')) {
                win.classList.remove('minimized');
                win.style.zIndex = ++zIndexCounter;
            } else {
                win.style.zIndex = ++zIndexCounter;
                // Add shake animation
                win.classList.remove('shake');
                void win.offsetWidth; // Force reflow
                win.classList.add('shake');
                setTimeout(() => {
                    win.classList.remove('shake');
                }, 500);
            }
            document.getElementById('launcher').classList.remove('active');
            return;
        }
    }

    const workspace = document.getElementById('workspace');

    // Close launcher
    document.getElementById('launcher').classList.remove('active');

    // Basic app window structure
    const win = document.createElement('div');
    win.className = `window ${type}-app`;
    win.style.top = '60px'; // Spawn below status bar
    win.style.left = '100px';
    win.style.width = type === 'native' ? '800px' : '600px';
    win.style.height = type === 'native' ? '500px' : '400px';
    win.style.zIndex = ++zIndexCounter;

    // Handle focus
    win.onmousedown = () => {
        if (!win.classList.contains('minimized')) {
            win.style.zIndex = ++zIndexCounter;
        }
    };

    win.innerHTML = `
        <div class="window-header">
            <span class="window-title">${escapeHTML(name)} ${customContent ? '' : '(' + type.toUpperCase() + ')'}</span>
            <div class="window-controls">
                <div class="control focus" title="Modo Focus" role="button" aria-label="Cambiar a modo enfoque" tabindex="0" onclick="toggleFocusMode(this)" onkeydown="if(event.key==='Enter') toggleFocusMode(this)"></div>
                <div class="control minimize" title="Minimizar" role="button" aria-label="Minimizar ventana" tabindex="0" onclick="minimizeWindow(this)" onkeydown="if(event.key==='Enter') minimizeWindow(this)"></div>
                <div class="control maximize" title="Maximizar" role="button" aria-label="Maximizar ventana" tabindex="0" onclick="maximizeWindow(this)" onkeydown="if(event.key==='Enter') maximizeWindow(this)">
                    <div class="snap-menu">
                        <div class="snap-option layout-split" role="button" aria-label="Dividir izquierda 50%" tabindex="0" onclick="snapWindow(this, 'left-50', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'left-50', event)">
                            <div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                        <div class="snap-option layout-split" role="button" aria-label="Dividir derecha 50%" tabindex="0" onclick="snapWindow(this, 'right-50', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'right-50', event)">
                            <div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                        <div class="snap-option layout-quad" role="button" aria-label="Cuadrante superior izquierdo" tabindex="0" onclick="snapWindow(this, 'tl-25', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'tl-25', event)">
                            <div class="snap-box"></div><div class="snap-box"></div>
                            <div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                        <div class="snap-option layout-70-30" role="button" aria-label="Dividir izquierda 70%" tabindex="0" onclick="snapWindow(this, 'left-70', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'left-70', event)">
                            <div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                        <div class="snap-option layout-3-col" role="button" aria-label="Tres columnas" tabindex="0" onclick="snapWindow(this, 'mid-33', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'mid-33', event)">
                            <div class="snap-box"></div><div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                        <div class="snap-option layout-quad" role="button" aria-label="Cuadrante superior derecho" tabindex="0" onclick="snapWindow(this, 'tr-25', event)" onkeydown="if(event.key==='Enter') snapWindow(this, 'tr-25', event)">
                            <div class="snap-box"></div><div class="snap-box"></div>
                            <div class="snap-box"></div><div class="snap-box"></div>
                        </div>
                    </div>
                </div>
                <div class="control close" title="Cerrar" role="button" aria-label="Cerrar ventana" tabindex="0" onclick="closeWindow(this)" onkeydown="if(event.key==='Enter') closeWindow(this)"></div>
            </div>
        </div>
        <div class="window-content" style="height: calc(100% - 40px); overflow: hidden;">
            ${customContent || `
            <div style="padding: 20px; color: #94a3b8; font-family: monospace;">
                Initializing ${escapeHTML(name)} subsystem...<br>
                > Loading shared libraries for ${type} kernel...<br>
                > Mounting filesystem...<br>
                > GUI handoff complete.<br><br>
                <div style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; border: 1px dashed rgba(255,255,255,0.1);">
                    <strong>${name} Interface</strong><br>
                    Concepto de convergencia: ${type === 'linux' ? 'Desktop Linux Application' : 'Android APK Runtime'}
                </div>
            </div>
            `}
        </div>
        <!-- Resize Handles -->
        <div class="resizer resizer-r"></div>
        <div class="resizer resizer-b"></div>
        <div class="resizer resizer-l"></div>
        <div class="resizer resizer-t"></div>
        <div class="resizer resizer-rb"></div>
        <div class="resizer resizer-lb"></div>
        <div class="resizer resizer-rt"></div>
        <div class="resizer resizer-lt"></div>
    `;

    workspace.appendChild(win);

    // Focus management for accessibility
    win.setAttribute('tabindex', '-1');
    win.focus();

    // Simple drag logic (basic concept)
    makeDraggable(win);
    makeResizable(win);
}

// Resizing Logic
function makeResizable(win) {
    const resizers = win.querySelectorAll('.resizer');
    let isResizing = false;

    resizers.forEach(resizer => {
        resizer.onmousedown = (e) => {
            isResizing = true;
            e.preventDefault();
            e.stopPropagation();

            const startWidth = win.offsetWidth;
            const startHeight = win.offsetHeight;
            const startX = e.clientX;
            const startY = e.clientY;
            const startTop = win.offsetTop;
            const startLeft = win.offsetLeft;

            // ⚡ Bolt: Use requestAnimationFrame for smoother resizing.
            // This prevents layout thrashing by throttling updates to the screen refresh rate (60fps),
            // rather than firing on every mousemove event which can be much faster.
            let rafId = null;
            let currentX = startX;
            let currentY = startY;

            const updateSize = () => {
                if (!isResizing) return;

                if (resizer.classList.contains('resizer-r')) {
                    win.style.width = startWidth + (currentX - startX) + 'px';
                } else if (resizer.classList.contains('resizer-b')) {
                    win.style.height = startHeight + (currentY - startY) + 'px';
                } else if (resizer.classList.contains('resizer-l')) {
                    const newWidth = startWidth - (currentX - startX);
                    if (newWidth > 200) {
                        win.style.width = newWidth + 'px';
                        win.style.left = startLeft + (currentX - startX) + 'px';
                    }
                } else if (resizer.classList.contains('resizer-t')) {
                    const newHeight = startHeight - (currentY - startY);
                    if (newHeight > 100) {
                        win.style.height = newHeight + 'px';
                        win.style.top = startTop + (currentY - startY) + 'px';
                    }
                } else if (resizer.classList.contains('resizer-rb')) {
                    win.style.width = startWidth + (currentX - startX) + 'px';
                    win.style.height = startHeight + (currentY - startY) + 'px';
                } else if (resizer.classList.contains('resizer-lb')) {
                    const newWidth = startWidth - (currentX - startX);
                    if (newWidth > 200) {
                        win.style.width = newWidth + 'px';
                        win.style.left = startLeft + (currentX - startX) + 'px';
                    }
                    win.style.height = startHeight + (currentY - startY) + 'px';
                } else if (resizer.classList.contains('resizer-rt')) {
                    win.style.width = startWidth + (currentX - startX) + 'px';
                    const newHeight = startHeight - (currentY - startY);
                    if (newHeight > 100) {
                        win.style.height = newHeight + 'px';
                        win.style.top = startTop + (currentY - startY) + 'px';
                    }
                } else if (resizer.classList.contains('resizer-lt')) {
                    const newWidth = startWidth - (currentX - startX);
                    const newHeight = startHeight - (currentY - startY);
                    if (newWidth > 200) {
                        win.style.width = newWidth + 'px';
                        win.style.left = startLeft + (currentX - startX) + 'px';
                    }
                    if (newHeight > 100) {
                        win.style.height = newHeight + 'px';
                        win.style.top = startTop + (currentY - startY) + 'px';
                    }
                }
                rafId = null;
            };

            const handleResize = (e) => {
                if (!isResizing) return;
                currentX = e.clientX;
                currentY = e.clientY;

                if (!rafId) {
                    rafId = requestAnimationFrame(updateSize);
                }
            };

            const stopResize = () => {
                isResizing = false;
                if (rafId) {
                    cancelAnimationFrame(rafId);
                    rafId = null;
                }
                window.removeEventListener('mousemove', handleResize);
                window.removeEventListener('mouseup', stopResize);
            };

            window.addEventListener('mousemove', handleResize);
            window.addEventListener('mouseup', stopResize);
        };
    });
}

function closeWindow(btn) {
    btn.closest('.window').remove();
}

function maximizeWindow(btn) {
    const win = btn.closest('.window');
    const shell = document.querySelector('.os-shell');
    const appNameDisplay = document.querySelector('.active-app-name');
    const appTitle = win.querySelector('.window-title').innerText.split('(')[0].trim();

    win.classList.remove('snapped');
    win.classList.toggle('maximized');

    if (win.classList.contains('maximized')) {
        shell.classList.add('window-full');
        appNameDisplay.innerHTML = `<span style="opacity:0.25; margin: 0 12px; font-weight: 300;">|</span> ${appTitle}`;
        appNameDisplay.style.color = 'var(--accent)';
        win.style.zIndex = 9005; // Below status text (10000)
    } else {
        shell.classList.remove('window-full');
        appNameDisplay.innerText = 'Escritorio';
        appNameDisplay.style.color = 'white';
        win.style.zIndex = ++zIndexCounter;
    }
}

function minimizeWindow(btn) {
    const win = btn.closest('.window');
    win.classList.add('minimized');
}

function toggleFocusMode(btn) {
    const win = btn.closest('.window');
    const shell = document.querySelector('.os-shell');

    win.classList.toggle('focus-mode');
    shell.classList.toggle('hide-chrome');

    // If entering focus mode, make it top-most
    if (win.classList.contains('focus-mode')) {
        win.style.zIndex = 9999;
    } else {
        win.style.zIndex = ++zIndexCounter;
    }
}

function snapWindow(btn, region, e) {
    if (e) e.stopPropagation();
    const win = btn.closest('.window');
    win.classList.add('snapped');
    win.classList.remove('maximized');

    const sb = 40; // Status bar approx

    switch (region) {
        case 'left-50':
            win.style.top = sb + 'px';
            win.style.left = '0';
            win.style.width = '50vw';
            win.style.height = `calc(100vh - ${sb}px - 64px)`;
            break;
        case 'right-50':
            win.style.top = sb + 'px';
            win.style.left = '50vw';
            win.style.width = '50vw';
            win.style.height = `calc(100vh - ${sb}px - 64px)`;
            break;
        case 'tl-25':
            win.style.top = sb + 'px';
            win.style.left = '0';
            win.style.width = '50vw';
            win.style.height = `calc(50vh - ${sb / 2}px - 32px)`;
            break;
        case 'tr-25':
            win.style.top = sb + 'px';
            win.style.left = '50vw';
            win.style.width = '50vw';
            win.style.height = `calc(50vh - ${sb / 2}px - 32px)`;
            break;
        case 'left-70':
            win.style.top = sb + 'px';
            win.style.left = '0';
            win.style.width = '70vw';
            win.style.height = `calc(100vh - ${sb}px - 64px)`;
            break;
        case 'mid-33':
            win.style.top = sb + 'px';
            win.style.left = '33.33vw';
            win.style.width = '33.33vw';
            win.style.height = `calc(100vh - ${sb}px - 64px)`;
            break;
    }

    // Auto maximize if clicked the icon part? No, just snap.
}

function makeDraggable(el) {
    const header = el.querySelector('.window-header');
    let startX = 0, startY = 0;
    let initialLeft = 0, initialTop = 0;
    let rafId = null;

    // Track current drag state for RAF
    let currentLeft = 0;
    let currentTop = 0;

    header.onmousedown = dragMouseDown;
    header.ontouchstart = dragMouseDown;

    function getClientPos(e) {
        if (e.type.startsWith('touch')) {
            if (e.touches && e.touches.length > 0) {
                return { x: e.touches[0].clientX, y: e.touches[0].clientY };
            }
            return { x: 0, y: 0 };
        }
        return { x: e.clientX, y: e.clientY };
    }

    function dragMouseDown(e) {
        if (el.classList.contains('maximized') || el.classList.contains('focus-mode')) return;

        e = e || window.event;

        const pos = getClientPos(e);
        startX = pos.x;
        startY = pos.y;

        initialLeft = el.offsetLeft;
        initialTop = el.offsetTop;

        // Initialize current position
        currentLeft = initialLeft;
        currentTop = initialTop;

        document.onmouseup = closeDragElement;
        document.onmousemove = elementDrag;

        document.ontouchend = closeDragElement;
        document.ontouchmove = elementDrag;
    }

    function elementDrag(e) {
        e = e || window.event;

        const pos = getClientPos(e);

        // Calculate new position based on delta
        const dx = pos.x - startX;
        const dy = pos.y - startY;

        currentLeft = initialLeft + dx;
        currentTop = initialTop + dy;

        if (!rafId) {
            rafId = requestAnimationFrame(updatePosition);
        }
    }

    function updatePosition() {
        el.style.top = currentTop + "px";
        el.style.left = currentLeft + "px";
        rafId = null;
    }

    function closeDragElement() {
        if (rafId) {
            cancelAnimationFrame(rafId);
            rafId = null;
        }
        document.onmouseup = null;
        document.onmousemove = null;
        document.ontouchend = null;
        document.ontouchmove = null;
    }
}

function setupLauncherNav() {
    const search = document.getElementById('launcher-search');
    const getApps = () => Array.from(document.querySelectorAll('.launcher-item')).filter(e => e.style.display !== 'none');

    search.addEventListener('keydown', (e) => {
        const apps = getApps();
        if (e.key === 'ArrowDown' && apps.length) {
            e.preventDefault();
            apps[0].focus();
        }
        if (e.key === 'Enter' && apps.length) {
            e.preventDefault();
            apps[0].click();
        }
        if (e.key === 'Escape') {
            e.preventDefault();
            if (search.value.length > 0) {
                clearSearch();
            } else {
                toggleLauncher();
            }
        }
    });

    document.querySelectorAll('.launcher-item').forEach(item => {
        item.addEventListener('keydown', (e) => {
            const apps = getApps();
            const idx = apps.indexOf(item);
            if (['ArrowDown', 'ArrowRight'].includes(e.key)) {
                e.preventDefault();
                if (idx < apps.length - 1) apps[idx + 1].focus();
            } else if (['ArrowUp', 'ArrowLeft'].includes(e.key)) {
                e.preventDefault();
                if (idx > 0) apps[idx - 1].focus();
                else search.focus();
            } else if (e.key === 'Escape') {
                e.preventDefault();
                toggleLauncher();
            }
        });
    });
}

if (typeof module === 'undefined') {
    setupLauncherNav();
}

// Boot Splash Screen Logic
document.addEventListener('DOMContentLoaded', () => {
    const splash = document.getElementById('boot-splash');
    if (splash) {
        // Simulate boot time (e.g. 2.5 seconds)
        setTimeout(() => {
            splash.classList.add('fade-out');

            // Remove from DOM after transition
            setTimeout(() => {
                splash.remove();

                // UX: Hand focus to the OS logo (Start button) for accessibility
                const startBtn = document.querySelector('.os-logo');
                if (startBtn) {
                    startBtn.focus();
                }
            }, 600); // Match CSS transition duration
        }, 2500);
    }
});

if (typeof module !== 'undefined') {
    module.exports = {
        toggleEterMenu,
        toggleLauncher,
        toggleControlCenter,
        filterApps,
        spawnApp,
        closeWindow,
        maximizeWindow,
        minimizeWindow,
        toggleFocusMode,
        snapWindow
    };
}
let filterTimeout = null;
function filterAppsDebounced() {
    if (filterTimeout) clearTimeout(filterTimeout);
    filterTimeout = setTimeout(() => {
        if (typeof window.filterApps === 'function') {
            window.filterApps();
        } else {
            filterApps();
        }
    }, 200);
}
window.filterAppsDebounced = filterAppsDebounced;

document.querySelectorAll('.cc-slider').forEach(slider => {
    // Set initial aria-valuetext
    slider.setAttribute('aria-valuetext', slider.value + '%');

    // Add event listener to update span and aria-valuetext and icon opacity
    slider.addEventListener('input', (e) => {
        const val = e.target.value;
        const span = e.target.nextElementSibling;
        if (span && span.classList.contains('slider-value')) {
            span.textContent = val + '%';
        }
        e.target.setAttribute('aria-valuetext', val + '%');

        const icon = e.target.previousElementSibling;
        if (icon && icon.tagName === 'IMG') {
            // Opacity calculation: 0.3 + (val/100) * 0.7
            const opacity = 0.3 + (val / 100) * 0.7;
            icon.style.opacity = opacity;
        }
    });

    // Trigger input to set initial opacity
    slider.dispatchEvent(new Event('input'));
});

function clearNotifications() {
    const list = document.getElementById('notif-list');
    const empty = document.getElementById('notif-empty');
    if (list) list.style.display = 'none';
    if (empty) empty.style.display = 'block';
}

if (typeof module !== 'undefined') {
    module.exports.clearNotifications = clearNotifications;
} else {
    window.clearNotifications = clearNotifications;
}

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        const activeWindow = document.querySelector('.window.active-window') || document.querySelector('.window:last-of-type');
        if (activeWindow) {
            e.preventDefault();
            activeWindow.remove();
        }
    }
});
