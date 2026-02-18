# Eter OS Concept - Interfaz Conceptual

Este documento describe la nueva interfaz conceptual para Eter OS, diseñada para lograr la convergencia real entre dispositivos PC, Tablet y Mobile, permitiendo la ejecución visual de aplicaciones Linux y Android.

## Características Implementadas

### 1. Diseño Premium (Glassmorphism)
La interfaz utiliza efectos de desenfoque de fondo (`backdrop-filter: blur`), bordes sutiles y gradientes profundos para dar una sensación de sistema moderno y de gama alta.

### 2. Convergencia Adaptativa
*   **Desktop:** Ventanas flotantes, barra de estado superior y dock centrado.
*   **Tablet:** Layout optimizado para mayor espacio de interacción (puedes probarlo redimensionando el navegador a ~768px).
*   **Mobile:** Las ventanas se maximizan automáticamente (100% width/height) y el dock se expande para facilitar el uso táctil (~360px).

### 3. Ecosistema Unificado (Linux + Android)
*   **Linux Apps:** Bordes de ventana con acento azul (ej: GIMP, VS Code).
*   **Lanzador Expandido:**
    *   **Buscador Inteligente:** Barra de búsqueda superior que filtra apps por nombre o sistema (Android/Linux).
    *   **Más Apps y Scroll:** Parrilla de más de 14 aplicaciones con scroll fluido y barra de desplazamiento personalizada.

### 4. Modo Focus / Gaming
*   **Botón Púrpura:** Nuevo control en las ventanas que activa la "Pantalla Completa Real".
*   **Inmersión Total:** Al activarlo, el shell del sistema (Barra de estado y Dock) se ocultan automáticamente para dar prioridad absoluta a la aplicación o juego.
*   Todas las apps se lanzan desde un Unified Launcher unificado o desde el Dock.

### 5. Centro de Control y Notificaciones
*   **Panel de Ajustes Rápidos:** Al hacer clic en la zona del reloj/batería, se despliega un panel de ajustes rápidos (WiFi, Bluetooth, Brillo, Volumen) con un diseño "Glassmorphic" premium.
*   **Centro de Notificaciones:** Integrado en el mismo panel, muestra alertas conceptuales de aplicaciones tanto de Linux como de Android, manteniendo una estética unificada.

### 6. Guía de Convergencia
Se ha creado un documento estratégico "Checklist de Convergencia" que detalla los desafíos técnicos para integrar GIMP (Linux) y apps Android en un mismo entorno.

### 7. Gestión de Ventanas (Window Controls)
*   **Cerrar (Rojo):** Elimina la instancia de la aplicación.
*   **Maximizar (Verde):** Alterna entre el modo ventana y pantalla completa (ajustado a la barra de estado). Las ventanas maximizadas bloquean el arrastre para mayor estabilidad.
*   **Minimizar (Naranja):** Aplica una animación de escala y traslación hacia el dock (simulación de ocultado).

### 8. Unificación de Header (Barra de Estado Unificada)
*   **Modo Pantalla Completa:** Cuando una aplicación se maximiza, sus controles (puntos de colores) se integran directamente en la barra de estado superior.
*   **Identidad Dinámica:** El nombre de la aplicación activa aparece junto al logo de Eter OS con el color de énfasis del sistema, creando una transición fluida y profesional.
*   **Interacción Inteligente:** La barra de estado permite el paso de clics para que los botones de la ventana sigan siendo funcionales sin bloqueos.

### 9. Redimensionamiento Universal
*   **Control Total:** Las ventanas ahora pueden redimensionarse desde cualquier borde o esquina, permitiendo una gestión del espacio de trabajo similar a un sistema operativo de sobremesa completo.
*   **Restricciones Inteligentes:** El redimensionamiento se desactiva automáticamente en el modo maximizado o anclado (Snap) para mantener la integridad del layout.

## Archivos del Proyecto

Los archivos de la interfaz se encuentran en el directorio `web_ui/`:
*   `web_ui/index.html`: Estructura del shell.
*   `web_ui/style.css`: Sistema de diseño y media queries.
*   `web_ui/app.js`: Lógica de ventanas y lanzador.

## Cómo Probarlo

1.  Abre `web_ui/index.html` en tu navegador web.
2.  Usa el icono de "Menú" en el dock para abrir el Lanzador.
3.  Arrastra las ventanas (toma la barra superior).
4.  Redimensiona la ventana del navegador para ver cómo la interfaz pasa de modo Desktop a Mobile automáticamente.
