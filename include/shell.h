/**
 * éterOS - Shell Interactivo
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Terminal de comandos integrada en el kernel.
 * Provee una interfaz básica de línea de comandos.
 */

#ifndef ETEROS_SHELL_H
#define ETEROS_SHELL_H

#define SHELL_MAX_INPUT     256
#define SHELL_MAX_ARGS      16
#define SHELL_PROMPT        "eterOS"

/**
 * Inicia el loop principal del shell interactivo.
 * Esta función nunca retorna.
 */
void shell_run(void);

#endif /* ETEROS_SHELL_H */
