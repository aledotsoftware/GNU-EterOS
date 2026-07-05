import re

with open(".jaa/state.md", "r") as f:
    state = f.read()

update = """
- **graphics-power-panel-bot**: Graphical UI and Window Controls Tooltips updated.
  - Implemented CSS tooltips using `::before` pseudo-element for `.control` class in `web_ui/style.css` (Cerrar, Minimizar, Maximizar window controls) providing consistency across the web UI.
  - Aligned native C `marea_shell.c` by drawing hover tooltips directly in `draw_window_chrome()` based on hit detection logic (`hit_minimize_button`, etc.), matching web UI tooltips logic in the native stack.
"""

# Append update right after JAA Global System State title
state = re.sub(r'(# JAA Global System State - Agent Update\n)', r'\1' + update, state)

with open(".jaa/state.md", "w") as f:
    f.write(state)
