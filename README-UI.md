# Flux UI — Especificación Funcional de Superficie (eterOS)

> **Filosofía**: Flux UI no es un gestor de ventanas; es un motor de profundidad espacial. El sistema trata a las aplicaciones como nodos dentro de una Constelación y la navegación se rige exclusivamente por el eje de profundidad (Z).

---

## 1. El Paradigma Unificador: "Física Digital"

El desafío principal es la "Disociación del Input": Unificar la precisión del Mouse con la naturalidad del Touch sin crear dos sistemas operativos distintos.

### 1.1 El Campo de Influencia (The Influence Field)
Eliminamos el cursor tradicional y el botón como estado binario.

*   **PC (Luz Física)**: El cursor es un punto de luz tenue (orbe) que ejerce gravedad. Al acercarse, los elementos se deforman magnéticamente hacia él *antes* de ser clicados. Feedback anticipatorio.
*   **Touch (Presión de Superficie)**: El dedo no es una coordenada X/Y, es un área de presión. La interfaz se "hunde" en el eje Z al tocar. La acción se dispara al soltar (Release), permitiendo cancelación analógica deslizando fuera.

### 1.2 Materialidad Adaptativa
La estética comunica estado, no decoración.

*   **Iluminación Diegética**: 
    *   *Mouse*: La luz sigue al cursor.
    *   *Touch*: La luz emana del último punto de contacto.
    *   *Inactivo*: Estado "Sueño Profundo" (OLED Black), solo pulsan los **Ecos** (notificaciones).
*   **Tipografía Fluida**: Uso de Variable Fonts que ajustan su peso (Weight) y ancho (Width) según la distancia de visualización y el nivel de Zoom.

---

## 2. Navegación: "Infinite Zoom"

Hemos eliminado el "doble clic" y la "X". El usuario habita un espacio y se desplaza en profundidad.

### 2.1 Zoom Semántico (Entrada y Salida)
| Acción | Gesto Touch | Control PC (Mouse/Teclado) | Visual |
| :--- | :--- | :--- | :--- |
| **Entrar (Focus)** | Pinch Out (Expandir) | Rueda Arriba / Flecha Arriba | El nodo se materializa por densidad de datos. |
| **Salir (Constellation)** | Pinch In (Contraer) | Rueda Abajo / Flecha Abajo | La app se encoge a su posición en la Constelación. |
| **Navegar (Pan)** | Swipe 3 Dedos | Middle Click + Drag / Alt+Scroll | Desplazamiento lateral entre Cúmulos. |

### 2.2 Pre-escucha (Hover-State)
Al mantener el cursor o el dedo levemente sobre un nodo, este muestra un **Stream de Datos Vivo**: código ejecutándose, video en tiempo real, o métricas, sin necesidad de abrir la app.

---

## 3. Multitarea: Topología y Clusters

Rompemos con la "Ventana Flotante". Usamos **Cúmulos (Clusters)** y **Partición Espacial**.

### 3.1 Cúmulos
Las apps se organizan en grupos lógicos (ej. "Código + Terminal").
*   **Modo Foco (Mono)**: 100% de atención.
*   **Modo Dual (Duo)**: 50/50. Unión magnética ("tensión superficial") entre apps.
*   **Satélites**: Micro-apps (Calculadora, Notas) que ESTAN EN SUPERPOCICION FLOTANDO SOBRE el Cúmulo principal.

### 3.2 Partición Dinámica (The Flux Frame)
El sistema sugiere divisiones matemáticas perfectas (Smart Snap).
*   **Divisor Fantasma**: En Touch, no necesitas tocar la línea exacta de separación. Tocar ambas apps y arrastrar ajusta la proporción ("tira y afloja").

---

## 4. Ecos de Sistema (Notificaciones)

Las alertas son perturbaciones en la superficie, no pop-ups.

*   **Eco (Sutil)**: Pulso de luz en el borde (Cyan = Info, Ámbar = Alerta).
*   **Rizo (Activo)**: El contenido se desplaza orgánicamente para revelar una pestaña en el borde.
*   **Interacción**: Zoom In sobre el Eco abre el detalle. Flick (deslizar) lo descarta.

---

## 5. El "No-File" System (Persistencia Semántica)

eterOS no usa carpetas. Usa **Flujos de Datos**.

*   **Búsqueda por Zoom Out**: Al alejar al máximo, los datos aparecen como una nebulosa. El tamaño del punto indica relevancia/recencia.
*   **Contexto**: Los assets se agrupan por "con qué se usaron". (Ej: Un .js y un .png orbitan juntos si se usaron en la misma sesión).

---

## 6. Seguridad: Materialización Biométrica (NO IMPLEMENTAR AUN)

El acceso es una proyección de identidad. No hay Login Box.

*   **Desbloqueo**: Al detectar presencia, el núcleo pulsa. Un gesto o mirada "materializa" los nodos privados.
*   **Privacidad Contextual**: Si se detecta un observador no autorizado, los datos sensibles sufren un desenfoque (Blur) automático.

---

## 7. Requerimientos de Kernel (Implementación Técnica)

Para que Flux UI sea real, el Kernel debe garantizar:

1.  **Compositor de Latencia Zero**: Zoom instantáneo (60fps min). Renderizado de "Skeleton" si la app no responde.
2.  **Aislamiento de Recursos**: Cada nodo en su propio sandbox visual. Si falla, se "rompe" gráficamente (glitch), pero la Constelación persiste.
3.  **Abstracción de Hardware**: Pixel shifting orgánico para proteger paneles OLED.

---

## 8. Física Digital (Micro-Interacciones)

Para que el sistema se sienta vivo y no mecánico, estas leyes físicas son obligatorias en cada interacción:

### 8.1 Magnetismo (The Influence Field)
El cursor/dedo tiene masa y gravedad.
*   **Atracción**: Los botones y nodos interactivos deben desplazarse sutilmente hacia el cursor cuando este se acerca (radio de influencia).
*   **Deformación**: El borde del elemento se estira hacia el punto de atracción antes de ser clicado. Esto elimina la necesidad de apuntar con precisión de píxel.

### 8.2 Inercia (Momentum)
Nada se detiene en seco.
*   **Scroll/Pan**: Al soltar el gesto, el movimiento continúa y desacelera por fricción simulada.
*   **Arrastre**: Los elementos arrastrados tienen "peso". Si lanzas una ventana, esta se desliza antes de detenerse.

### 8.3 Elasticidad (Springs)
*   **Límites**: Al llegar al final de una lista o borde, el contenido se estira y rebota (Rubber-banding).
*   **Zoom**: La transición de entrada/salida no es lineal; tiene un micro-rebote al final (Overshoot) para confirmar la llegada.

### 8.4 Resistencia (Friction)
*   **Snapping**: Mover un objeto cerca de una zona de anclaje (Split) debe sentirse "pegajoso". El cursor se ralentiza magnéticamente para ayudar a soltar.

---

---

## 9. Modelo Cognitivo (Aprendizaje y Descubribilidad)

Este sistema rompe 40 años de hábitos. La UX debe enseñar el Eje Z.

### 9.1 Onboarding Espacial
*   **Fantasmas**: Clones translúcidos de la mano/cursor que demuestran gestos complejos antes de que el usuario los necesite.
*   **Profundidad Asistida**: Al inicio, la paralaje es exagerada para evidenciar que hay capas.
*   **Modo Clásico Asistido**: Durante las primeras 10 horas de uso, aparecen "rieles" visuales que guían hacia los gestos correctos. Se desvanecen con la competencia.

---

## 10. Estados Globales del Sistema

La Constelación respira y muta según la salud del sistema.

*   **Offline**: Los nodos se vuelven monocromáticos y pierden brillo (Wireframe).
*   **Batería Baja (<20%)**: La interfaz pierde profundidad (se aplana) para ahorrar GPU. El "Lumen" del cursor se atenúa.
*   **Error Global**: La Constelación tiembla levemente (glitch sutil). El nodo fallido se fragmenta.

---

## 11. Principio Rector: Reducción por Estrés (Adaptive Complexity)

Un sistema espacial debe saber cuándo volverse plano.

> **Regla de Oro**: Cuando el sistema (CPU/RAM) o el Usuario están bajo presión, la interfaz se simplifica, no se embellece.

*   **Estrés de Hardware**: Si FPS < 60, desactivar Blur, Sombras y Física. Volver a transiciones lineales.
*   **Estrés de Usuario**: Si hay >5 notificaciones sin leer o movimiento errático del cursor, el sistema reduce la cantidad de elementos en pantalla (Focus Mode forzado).

---

## 12. Escalabilidad y XR

Flux UI es nativo para Realidad Extendida.

*   **XR/VR**: El eje Z deja de ser una metáfora y se vuelve real. Los Cúmulos flotan a distancias ergonómicas.
*   **Plegables**: Al plegar, la "tensión superficial" rompe el Cúmulo en dos.
*   **Pantallas Extremas**:
    *   *4"*: Solo 1 Cúmulo visible. Navegación puramente secuencial.
    *   *55"*: Múltiples Cúmulos activos simultáneos (Wall Mode).

---

## 13. Identidad Fluida (Sin Login Box)

*   **Sesión Efímera**: "Invitado" se activa si no se reconoce biometría. Solo acceso a apps públicas.
*   **Handoff Biométrico**: Si otro usuario registrado toma el dispositivo, la Constelación "rota" y muestra sus nodos privados sin cerrar sesión, solo ocultando la anterior.

---

## Roadmap Visual (Próximos Pasos)

### Fase 0: Base Técnica (Kernel Support) ✅
1.  [x] **Framebuffer Lineal**: Acceso directo a VRAM (32-bit color).
2.  [x] **Doble Buffer**: Renderizado off-screen para eliminar parpadeo.
3.  [x] **Primitivas de Dibujo**: Rectángulos, Líneas, Texto (Bitmap Font).
4.  [x] **Carga de Assets**: Lectura de imágenes RAW desde Initrd.

### Fase 1: Interacción Básica
1.  **Cursor por Software**: Dibujar puntero del mouse sobre el frame usando composición Alpha (sin hardware sprite).
2.  **Prototipo "Toque Magnético"**: Implementar la deformación de botones al acercar el cursor.
3.  **Sistema de Ventanas (Window Manager)**: Estructura de árbol para manejar redibujado y clipping.
4.  **Zoom de Texto**: Implementar LOD (Level of Detail) en tipografía.
3.  **Transición de Gravedad**: Rotación de layout fluido en el demo.