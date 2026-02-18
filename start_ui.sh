#!/bin/bash

# Simple script to launch the Eter OS Concept UI

if command -v python3 &>/dev/null; then
    echo "Iniciando servidor en http://localhost:8000..."
    echo "Presiona Ctrl+C para detener."
    python3 -m http.server 8000 --directory web_ui
elif command -v python &>/dev/null; then
    echo "Iniciando servidor en http://localhost:8000..."
    echo "Presiona Ctrl+C para detener."
    python -m http.server 8000 --directory web_ui
else
    echo "Python no encontrado. Por favor abre 'web_ui/index.html' directamente en tu navegador."
    exit 1
fi