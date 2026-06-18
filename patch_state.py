import re

with open('.jaa/state.md', 'r') as f:
    content = f.read()

content = content.replace("- [GENERAL] Estandarización de agentes para todos los repositorios.", "- [GENERAL] Estandarización de agentes para todos los repositorios.\n- [ETEROS] Comando DHCP, lwIP mailboxes y red integrados correctamente.")

with open('.jaa/state.md', 'w') as f:
    f.write(content)
