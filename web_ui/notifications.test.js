const { clearNotifications } = require('./app');

describe('clearNotifications', () => {
    let list;
    let clearBtn;
    let emptyState;

    beforeEach(() => {
        // Mock DOM structure
        document.body.innerHTML = `
            <div class="cc-notifications">
                <div class="cc-notifications-header">
                    <h4>Notificaciones</h4>
                    <button id="clear-notifs" aria-label="Borrar notificaciones">Borrar</button>
                </div>
                <div id="notif-list" class="notif-list">
                    <div class="notif">Notification 1</div>
                    <div class="notif">Notification 2</div>
                </div>
                <div id="notif-empty" class="notif-empty" hidden tabindex="-1">Sin notificaciones nuevas</div>
            </div>
        `;

        list = document.getElementById('notif-list');
        clearBtn = document.getElementById('clear-notifs');
        emptyState = document.getElementById('notif-empty');
    });

    test('should clear the list of notifications', () => {
        expect(list.children.length).toBe(2);
        clearNotifications();
        expect(list.children.length).toBe(0);
        expect(list.innerHTML).toBe('');
    });

    test('should hide the clear button', () => {
        expect(clearBtn.hidden).toBe(false);
        clearNotifications();
        expect(clearBtn.hidden).toBe(true);
    });

    test('should show the empty state', () => {
        expect(emptyState.hidden).toBe(true);
        clearNotifications();
        expect(emptyState.hidden).toBe(false);
    });

    test('should focus the empty state for accessibility', () => {
        const spy = jest.spyOn(emptyState, 'focus');
        clearNotifications();
        expect(spy).toHaveBeenCalled();
    });

    test('should handle missing elements gracefully', () => {
        document.body.innerHTML = ''; // Empty DOM
        expect(() => clearNotifications()).not.toThrow();
    });
});
