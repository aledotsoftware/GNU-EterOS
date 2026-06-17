import re

with open('.jaa/state.md', 'r') as f:
    state = f.read()

# Add bullet point to ota-update-panel-bot
bullet = "  - Hardened OTA verification by strictly enforcing Ed25519 payload signatures, removing bypass toggles, and adding bounds-validation to NVRAM state readers to prevent fallback loops when NVRAM is uninitialized."

if "- **ota-update-panel-bot**: OTA logic hardened and verified." in state:
    state = state.replace(
        "- **ota-update-panel-bot**: OTA logic hardened and verified.",
        f"- **ota-update-panel-bot**: OTA logic hardened and verified.\n{bullet}"
    )

with open('.jaa/state.md', 'w') as f:
    f.write(state)
