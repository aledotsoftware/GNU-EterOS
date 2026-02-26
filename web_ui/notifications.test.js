const { clearNotifications } = require('./app');

describe('Notification Management', () => {
    beforeEach(() => {
        // Mock DOM
        document.body.innerHTML = `
            <div class="cc-notifications">
                <div class="cc-notifications-header">
                    <h4>Notificaciones</h4>
                    <button id="clear-notifs">Borrar</button>
                </div>
                <div id="notif-list">
                    <div class="notif">Notification 1</div>
                    <div class="notif">Notification 2</div>
                </div>
                <div id="notif-empty" hidden>Sin notificaciones nuevas</div>
            </div>
        `;
    });

    test('clearNotifications should empty the list and toggle visibility', () => {
        clearNotifications();

        const list = document.getElementById('notif-list');
        const empty = document.getElementById('notif-empty');
        const btn = document.getElementById('clear-notifs');

        // Assertions
        expect(list.children.length).toBe(0); // List should be emptied
        expect(list.style.display).toBe('none'); // List should be hidden
        expect(empty.hidden).toBe(false); // Empty state should be visible
        expect(btn.style.display).toBe('none'); // Button should be hidden
    });
});
