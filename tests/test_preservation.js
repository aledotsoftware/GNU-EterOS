#!/usr/bin/env node

/**
 * Test de Propiedad de Preservación - Contenido Técnico Correcto Existente
 *
 * **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5**
 *
 * IMPORTANTE: Estos tests deben pasar en el código original y seguir pasando después de las correcciones.
 */

const fs = require('fs');
const path = require('path');

// Colores para output
const RED = '\x1b[31m';
const GREEN = '\x1b[32m';
const YELLOW = '\x1b[33m';
const RESET = '\x1b[0m';

let failed = false;

function assertContains(filePath, pattern, description) {
  try {
    const content = fs.readFileSync(path.join(__dirname, '..', filePath), 'utf8');
    if (content.includes(pattern)) {
      console.log(`${GREEN}✓ PASA: ${description}${RESET}`);
    } else {
      console.log(`${RED}✗ FALLO: ${description} (no se encontró el patrón)${RESET}`);
      failed = true;
    }
  } catch (err) {
    console.log(`${RED}✗ FALLO: ${description} (no se pudo leer el archivo ${filePath})${RESET}`);
    failed = true;
  }
}

console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
console.log(`${YELLOW}Test de Preservación de Contenido Técnico${RESET}`);
console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);

// README.md sections
assertContains('README.md', '## 2. Arquitectura General (Ether-Core)', 'Sección Ether-Core en README.md');
assertContains('README.md', '## 4. Gestión de Memoria (MMU)', 'Sección MMU en README.md');
assertContains('README.md', '## 5. Planificador, SMP y Sincronización', 'Sección SMP en README.md');

// Exact READMEs
assertContains('kernel/arch/README.md', 'Cada subdirectorio contiene la implementación de la HAL', 'Contenido de kernel/arch/README.md');
assertContains('boot/common/README.md', 'Este directorio contendrá código compartido', 'Contenido de boot/common/README.md');

// Code comments
assertContains('include/hal.h', 'Hardware Abstraction Layer (HAL)', 'Comentarios en include/hal.h');
assertContains('include/syscall.h', 'Linux x86_64 Syscall Numbers', 'Comentarios en include/syscall.h');

console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);

if (failed) {
  console.log(`${RED}✗ ALGUNOS TESTS DE PRESERVACIÓN FALLARON${RESET}`);
  process.exit(1);
} else {
  console.log(`${GREEN}✓ TODOS LOS TESTS DE PRESERVACIÓN PASARON${RESET}`);
  process.exit(0);
}
