#!/usr/bin/env node

/**
 * Test de Exploración de Condición de Bug - Documentación Desactualizada, Faltante e Incompleta
 * 
 * **Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8**
 * 
 * CRÍTICO: Este test DEBE FALLAR en código sin corregir - el fallo confirma que el bug existe
 * NO intentar corregir el test o el código cuando falle
 * OBJETIVO: Identificar contraejemplos que demuestran que la documentación es defectuosa
 * 
 * Property 1: Bug Condition - Documentación Desactualizada, Faltante e Incompleta
 * 
 * Este test verifica la condición de bug:
 * isBugCondition(D) = (D.isOutdated = true) OR 
 *                     (D.exists = false AND D.isRequired = true) OR
 *                     (D.isIncomplete = true AND D.isCritical = true)
 */

const fs = require('fs');
const path = require('path');
const assert = require('assert');

// Colores para output
const RED = '\x1b[31m';
const GREEN = '\x1b[32m';
const YELLOW = '\x1b[33m';
const RESET = '\x1b[0m';

// Contador de contraejemplos
let counterexamples = [];

/**
 * Verifica si un archivo existe
 */
function fileExists(filePath) {
  try {
    return fs.existsSync(filePath);
  } catch (err) {
    return false;
  }
}

/**
 * Lee el contenido de un archivo
 */
function readFile(filePath) {
  try {
    return fs.readFileSync(filePath, 'utf8');
  } catch (err) {
    return null;
  }
}

/**
 * Test 1: Verificar información desactualizada en README.md
 */
function testOutdatedInformation() {
  console.log('\n📋 Test 1: Información Desactualizada en README.md');
  
  const readmePath = path.join(__dirname, '..', 'README.md');
  const content = readFile(readmePath);
  
  if (!content) {
    counterexamples.push('README.md no existe o no se puede leer');
    console.log(`${RED}✗ FALLO: README.md no existe${RESET}`);
    return false;
  }
  
  // Verificar copyright 2026 (información desactualizada)
  if (content.includes('2026')) {
    counterexamples.push('README.md contiene "2026" (copyright desactualizado)');
    console.log(`${RED}✗ FALLO: README.md contiene "2026" (información desactualizada)${RESET}`);
    return false;
  }
  
  console.log(`${GREEN}✓ PASA: README.md no contiene información desactualizada${RESET}`);
  return true;
}

/**
 * Test 2: Verificar documentación faltante en módulos del kernel
 */
function testMissingModuleDocumentation() {
  console.log('\n📋 Test 2: Documentación Faltante en Módulos del Kernel');
  
  const modules = [
    'kernel/drivers/README.md',
    'kernel/fs/README.md',
    'kernel/net/README.md',
    'kernel/gfx/README.md',
    'kernel/crypto/README.md',
    'kernel/mm/README.md'
  ];
  
  let allExist = true;
  
  for (const modulePath of modules) {
    const fullPath = path.join(__dirname, '..', modulePath);
    if (!fileExists(fullPath)) {
      counterexamples.push(`${modulePath} NO existe (documentación faltante)`);
      console.log(`${RED}✗ FALLO: ${modulePath} NO existe${RESET}`);
      allExist = false;
    } else {
      console.log(`${GREEN}✓ PASA: ${modulePath} existe${RESET}`);
    }
  }
  
  return allExist;
}

/**
 * Test 3: Verificar documentación faltante de APIs
 */
function testMissingAPIDocumentation() {
  console.log('\n📋 Test 3: Documentación Faltante de APIs');
  
  const apiDocs = [
    'docs/api/hal.md',
    'docs/api/vfs.md',
    'docs/api/syscalls.md',
    'docs/api/drivers.md'
  ];
  
  let allExist = true;
  
  for (const apiPath of apiDocs) {
    const fullPath = path.join(__dirname, '..', apiPath);
    if (!fileExists(fullPath)) {
      counterexamples.push(`${apiPath} NO existe (API sin documentar)`);
      console.log(`${RED}✗ FALLO: ${apiPath} NO existe${RESET}`);
      allExist = false;
    } else {
      console.log(`${GREEN}✓ PASA: ${apiPath} existe${RESET}`);
    }
  }
  
  return allExist;
}

/**
 * Test 4: Verificar guías de desarrollo faltantes
 */
function testMissingDevelopmentGuides() {
  console.log('\n📋 Test 4: Guías de Desarrollo Faltantes');
  
  const guides = [
    'CONTRIBUTING.md',
    'docs/porting-guide.md',
    'docs/eterland.md',
    'docs/filesystem.md'
  ];
  
  let allExist = true;
  
  for (const guidePath of guides) {
    const fullPath = path.join(__dirname, '..', guidePath);
    if (!fileExists(fullPath)) {
      counterexamples.push(`${guidePath} NO existe (guía faltante)`);
      console.log(`${RED}✗ FALLO: ${guidePath} NO existe${RESET}`);
      allExist = false;
    } else {
      console.log(`${GREEN}✓ PASA: ${guidePath} existe${RESET}`);
    }
  }
  
  return allExist;
}

/**
 * Ejecutar todos los tests
 */
function runAllTests() {
  console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
  console.log(`${YELLOW}Test de Exploración de Condición de Bug - Documentación${RESET}`);
  console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
  
  const results = [
    testOutdatedInformation(),
    testMissingModuleDocumentation(),
    testMissingAPIDocumentation(),
    testMissingDevelopmentGuides()
  ];
  
  const allPassed = results.every(r => r === true);
  
  console.log(`\n${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
  console.log(`${YELLOW}Resumen de Contraejemplos Encontrados${RESET}`);
  console.log(`${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
  
  if (counterexamples.length > 0) {
    console.log(`\n${RED}Se encontraron ${counterexamples.length} contraejemplos que demuestran el bug:${RESET}\n`);
    counterexamples.forEach((example, index) => {
      console.log(`${RED}${index + 1}. ${example}${RESET}`);
    });
  } else {
    console.log(`\n${GREEN}No se encontraron contraejemplos - la documentación está completa y actualizada${RESET}`);
  }
  
  console.log(`\n${YELLOW}═══════════════════════════════════════════════════════════════${RESET}`);
  
  if (allPassed) {
    console.log(`${GREEN}✓ TODOS LOS TESTS PASARON - El bug está corregido${RESET}`);
    process.exit(0);
  } else {
    console.log(`${RED}✗ TESTS FALLARON - El bug existe (esto es esperado en código sin corregir)${RESET}`);
    process.exit(1);
  }
}

// Ejecutar tests
runAllTests();
