function updateClock() {
    const now = new Date();
    const timeString = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    document.getElementById('clock').textContent = timeString;
}

setInterval(updateClock, 1000);
updateClock();

function openWindow(appName) {
    const windowsContainer = document.getElementById('windows-container');
    const win = document.createElement('div');
    win.className = 'window';
    win.style.top = (50 + Math.random() * 50) + 'px';
    win.style.left = (50 + Math.random() * 50) + 'px';

    let content = '';
    if (appName === 'calculator') {
        content = '<h3>Calculator</h3><p>Simple Calculator App</p>';
    } else if (appName === 'notepad') {
        content = '<h3>Notepad</h3><textarea aria-label="Notepad" placeholder="Type your notes here..." style="width:100%; height:80%;"></textarea>';
    }

    win.innerHTML = `
        <div class="title-bar">
            <span>${appName.charAt(0).toUpperCase() + appName.slice(1)}</span>
            <div class="window-controls">
                <button aria-label="Close window" data-tooltip="Close" onclick="this.closest('.window').remove()">X</button>
            </div>
        </div>
        <div class="window-content">
            ${content}
        </div>
    `;

    makeDraggable(win);
    windowsContainer.appendChild(win);

    // Bring to front and set focus for accessibility
    win.dispatchEvent(new MouseEvent('mousedown'));
    const closeBtn = win.querySelector('button[aria-label="Close window"]');
    if (closeBtn) closeBtn.focus();
}

// Keyboard accessibility for icons
document.addEventListener('DOMContentLoaded', () => {
    const icons = document.querySelectorAll('.icon');
    icons.forEach(icon => {
        icon.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                icon.click();
            }
        });
    });

    // Global keyboard shortcuts
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            const windows = Array.from(document.querySelectorAll('.window'));
            if (windows.length > 0) {
                // Find active window (zIndex 10) or default to the last one
                const activeWindow = windows.find(w => w.style.zIndex === '10') || windows[windows.length - 1];
                activeWindow.remove();
            }
        }
    });
});

function makeDraggable(element) {
    let pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0;
    const header = element.querySelector('.title-bar');

    if (header) {
        header.onmousedown = dragMouseDown;
    }

    // Bring to front when any part of the window is clicked
    element.addEventListener('mousedown', () => {
        const windows = document.querySelectorAll('.window');
        windows.forEach(w => w.style.zIndex = 1);
        element.style.zIndex = 10;
    });

    function dragMouseDown(e) {
        e.preventDefault();
        pos3 = e.clientX;
        pos4 = e.clientY;
        document.onmouseup = closeDragElement;
        document.onmousemove = elementDrag;
    }

    function elementDrag(e) {
        e.preventDefault();
        pos1 = pos3 - e.clientX;
        pos2 = pos4 - e.clientY;
        pos3 = e.clientX;
        pos4 = e.clientY;

        element.style.top = (element.offsetTop - pos2) + "px";
        element.style.left = (element.offsetLeft - pos1) + "px";
    }

    function closeDragElement() {
        document.onmouseup = null;
        document.onmousemove = null;
    }
}
