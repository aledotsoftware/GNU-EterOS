# Documento de Requisitos de Bugfix: Documentación Desactualizada e Incompleta

## Introducción

La documentación del proyecto éterOS presenta múltiples deficiencias que dificultan la comprensión, uso y contribución al sistema operativo. El README.md principal contiene información desactualizada (referencias a copyright 2026, versión v0.2.0), mientras que áreas críticas del proyecto carecen completamente de documentación. Los subdirectorios principales (kernel/, drivers/, fs/, net/, gfx/) no tienen READMEs explicativos, las APIs públicas carecen de guías de uso, y no existen documentos para desarrolladores que deseen contribuir o portar éterOS a nuevas arquitecturas.

Este bugfix aborda sistemáticamente estas deficiencias para garantizar que la documentación refleje con precisión el estado actual del proyecto y proporcione la información necesaria para usuarios y desarrolladores.

## Análisis del Bug

### Comportamiento Actual (Defecto)

1.1 CUANDO un usuario lee el README.md principal ENTONCES el sistema muestra información desactualizada incluyendo copyright "2026" y referencias a versiones antiguas

1.2 CUANDO un desarrollador navega a directorios críticos (kernel/drivers/, kernel/fs/, kernel/net/, kernel/gfx/, kernel/crypto/, kernel/mm/) ENTONCES el sistema no proporciona ningún README explicativo sobre el propósito o estructura del módulo

1.3 CUANDO un desarrollador busca documentación de APIs públicas (HAL, VFS, syscalls, drivers) ENTONCES el sistema solo proporciona comentarios dispersos en headers sin guías de uso o ejemplos

1.4 CUANDO un contribuidor potencial busca guías de desarrollo ENTONCES el sistema no proporciona documentación sobre estándares de código, proceso de contribución, o arquitectura interna

1.5 CUANDO un desarrollador intenta portar éterOS a una nueva arquitectura ENTONCES el sistema no proporciona una guía paso a paso detallada más allá de comentarios básicos en hal.h

1.6 CUANDO un usuario busca instrucciones de compilación actualizadas ENTONCES el sistema muestra comandos que pueden no reflejar cambios recientes en el build system

1.7 CUANDO un desarrollador busca documentación del servidor gráfico Eterland ENTONCES el sistema no proporciona especificaciones del protocolo IPC, formato de mensajes, o API de composición

1.8 CUANDO un usuario busca documentación sobre el sistema de archivos soportado ENTONCES el sistema no documenta las limitaciones conocidas, formato de metadatos, o API del VFS

### Comportamiento Esperado (Correcto)

2.1 CUANDO un usuario lee el README.md principal ENTONCES el sistema DEBERÁ mostrar información actualizada con copyright correcto, versión actual, y estado preciso de características

2.2 CUANDO un desarrollador navega a directorios críticos (kernel/drivers/, kernel/fs/, kernel/net/, kernel/gfx/, kernel/crypto/, kernel/mm/) ENTONCES el sistema DEBERÁ proporcionar un README.md explicando propósito, estructura de archivos, y puntos de entrada principales

2.3 CUANDO un desarrollador busca documentación de APIs públicas ENTONCES el sistema DEBERÁ proporcionar guías en docs/api/ cubriendo HAL, VFS, syscalls, y drivers con ejemplos de uso

2.4 CUANDO un contribuidor potencial busca guías de desarrollo ENTONCES el sistema DEBERÁ proporcionar CONTRIBUTING.md con estándares de código, proceso de pull requests, y arquitectura del proyecto

2.5 CUANDO un desarrollador intenta portar éterOS a una nueva arquitectura ENTONCES el sistema DEBERÁ proporcionar docs/porting-guide.md con pasos detallados, checklist de funciones HAL, y ejemplos de implementaciones existentes

2.6 CUANDO un usuario busca instrucciones de compilación ENTONCES el sistema DEBERÁ proporcionar comandos verificados y actualizados en README.md y docs/building.md

2.7 CUANDO un desarrollador busca documentación del servidor gráfico Eterland ENTONCES el sistema DEBERÁ proporcionar docs/eterland.md con especificaciones del protocolo, API de composición, y ejemplos de clientes

2.8 CUANDO un desarrollador busca documentación sobre sistemas de archivos ENTONCES el sistema DEBERÁ proporcionar docs/filesystem.md documentando VFS, drivers (FAT32, JFS, InitRD), limitaciones, y API

### Comportamiento Sin Cambios (Prevención de Regresiones)

3.1 CUANDO un usuario lee secciones correctas del README.md (arquitectura Ether-Core, gestión de memoria, planificador SMP) ENTONCES el sistema DEBERÁ CONTINUAR mostrando esa información técnica precisa sin modificaciones

3.2 CUANDO un desarrollador lee los READMEs existentes (kernel/arch/README.md, boot/common/README.md) ENTONCES el sistema DEBERÁ CONTINUAR proporcionando esa información estructural sin pérdida de contenido

3.3 CUANDO un desarrollador lee comentarios en headers de código (include/hal.h, include/syscall.h) ENTONCES el sistema DEBERÁ CONTINUAR manteniendo esos comentarios inline sin eliminarlos

3.4 CUANDO el build system ejecuta scripts de compilación ENTONCES el sistema DEBERÁ CONTINUAR funcionando sin cambios en su comportamiento

3.5 CUANDO un usuario ejecuta comandos documentados que son correctos ENTONCES el sistema DEBERÁ CONTINUAR ejecutándolos exitosamente sin cambios

## Condición de Bug y Propiedad

### Función de Condición de Bug

```pascal
FUNCTION isBugCondition(D)
  INPUT: D of type DocumentationItem
  OUTPUT: boolean
  
  RETURN (D.isOutdated = true) OR 
         (D.exists = false AND D.isRequired = true) OR
         (D.isIncomplete = true AND D.isCritical = true)
END FUNCTION
```

Donde un `DocumentationItem` se considera defectuoso si:
- Contiene información desactualizada (fechas incorrectas, versiones antiguas, comandos obsoletos)
- No existe pero es requerido para áreas críticas (APIs públicas, guías de contribución, READMEs de módulos principales)
- Está incompleto y cubre funcionalidad crítica (instrucciones de compilación parciales, guías de porting sin pasos completos)

### Propiedad: Fix Checking

```pascal
// Propiedad: Corrección de Documentación
FOR ALL D WHERE isBugCondition(D) DO
  result ← updateDocumentation(D)
  ASSERT (result.isOutdated = false) AND
         (result.exists = true) AND
         (result.isComplete = true) AND
         (result.isAccurate = true)
END FOR
```

### Propiedad: Preservation Checking

```pascal
// Propiedad: Preservación de Documentación Correcta
FOR ALL D WHERE NOT isBugCondition(D) DO
  ASSERT contentBefore(D) = contentAfter(D)
END FOR
```

Esto asegura que la documentación que ya es correcta y precisa (como las secciones técnicas del README principal sobre arquitectura, memoria, y planificador) permanezca sin cambios.

## Ejemplos Concretos

### Ejemplo 1: Información Desactualizada
- **Input**: README.md línea "© 2026 **Tudex Networks**"
- **Condición de Bug**: `isOutdated = true` (año futuro incorrecto)
- **Comportamiento Actual**: Muestra copyright 2026
- **Comportamiento Esperado**: Muestra copyright del año actual

### Ejemplo 2: Documentación Faltante
- **Input**: Directorio `kernel/drivers/` sin README.md
- **Condición de Bug**: `exists = false AND isRequired = true`
- **Comportamiento Actual**: No hay documentación del módulo
- **Comportamiento Esperado**: README.md explicando drivers disponibles (e1000, serial, keyboard, mouse, etc.)

### Ejemplo 3: API Sin Documentar
- **Input**: Búsqueda de "cómo usar VFS API"
- **Condición de Bug**: `isIncomplete = true AND isCritical = true`
- **Comportamiento Actual**: Solo comentarios en include/fs/vfs.h
- **Comportamiento Esperado**: docs/api/vfs.md con ejemplos de mount, read_fs, write_fs, open_fs
