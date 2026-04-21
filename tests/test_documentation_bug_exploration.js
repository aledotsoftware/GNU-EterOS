#!/usr/bin/env node

/**
 * Test de Exploración de Condición de Bug - Documentación Desactualizada, Faltante e Incompleta
 * 
 * **Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8**
 * 
 * CRÍTICO: Este test DEBE FALLAR en código sin corregir - el fallo confirma que el bug existe
 * OBJETIVO: Identificar contraejemplos que demuestran que la documentación es defectuosa
 */

const fs = require('fs');
const path = require('path');

// Colores para output
const RED = '\x1b[31m';
const GREEN = '\x1b[32m';
const YELLOW = '\x1b[33m';
const RESET = '\x1b[0m';

let counterexamples = [];

function fileExists(filePath) {
  return fs.existsSync(path.join(__dirname, '..', filePath));
}

function readFile(filePath) {
  try {
    return fs.readFileSync(path.join(__dirname, '..', filePath), 'utf8');
  } catch (err) {
    return null;
  }
}

function checkBug(condition, message) {
  if (condition) {
    console.log(`${RED}✗ FALLO: ${message}${RESET}`);
    counterexamples.push(message);
    return false;
  }
  console.log(`${GREEN}✓ PASA: No se detectó bug: ${message.split(' (')[0]}${RESET}`);
  return true;
}

console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
console.log(`${YELLOW}Test de Exploración de Condición de Bug - Documentación${RESET}`);
console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);

const readmeContent = readFile('README.md');
// Bug 1.1: README.md contains "2026" (outdated).
// The goal is to reach 2025.
checkBug(readmeContent && readmeContent.includes('2026'), 'README.md contiene "2026" (información desactualizada)');

// Bug 1.2: Missing module READMEs
const modules = [
  'kernel/drivers/README.md',
  'kernel/fs/README.md',
  'kernel/net/README.md',
  'kernel/gfx/README.md',
  'kernel/crypto/README.md',
  'kernel/mm/README.md'
];

modules.forEach(m => {
  checkBug(!fileExists(m) || (readFile(m).trim().length === 0), `${m} NO existe o está vacío (documentación faltante)`);
});

// Bug 1.3: Missing API docs
const apiDocs = [
  'docs/api/hal.md',
  'docs/api/vfs.md',
  'docs/api/syscalls.md',
  'docs/api/drivers.md'
];
apiDocs.forEach(d => {
  checkBug(!fileExists(d), `${d} NO existe (API sin documentar)`);
});

// Bug 1.4, 1.5, 1.7, 1.8: Missing guides
const guides = [
  'CONTRIBUTING.md',
  'docs/porting-guide.md',
  'docs/eterland.md',
  'docs/filesystem.md'
];
guides.forEach(g => {
  checkBug(!fileExists(g), `${g} NO existe (guía faltante)`);
});

console.log(`\n${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
if (counterexamples.length > 0) {
  console.log(`${RED}✗ TESTS FALLARON - Se encontraron ${counterexamples.length} bugs.${RESET}`);
  process.exit(1);
} else {
  console.log(`${GREEN}✓ TODOS LOS TESTS PASARON - Documentación completa.${RESET}`);
  process.exit(0);
}
