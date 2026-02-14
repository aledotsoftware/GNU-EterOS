#ifndef FS_STAT_H
#define FS_STAT_H

#include <types.h>

/**
 * Estructura de información de archivo (stat).
 * Diseñada para ser robusta contra el problema Y2K38.
 */
struct stat {
    uint32_t st_dev;     /* ID del dispositivo conteniendo el archivo */
    uint32_t st_ino;     /* Número de Inodo */
    uint32_t st_mode;    /* Protección (permisos) */
    uint32_t st_nlink;   /* Número de enlaces físicos */
    uint32_t st_uid;     /* ID del usuario propietario */
    uint32_t st_gid;     /* ID del grupo propietario */
    uint32_t st_rdev;    /* ID del dispositivo (si es archivo especial) */
    uint64_t st_size;    /* Tamaño total en bytes */
    
    time_t   st_atime;   /* Tiempo del último acceso (64-bit) */
    time_t   st_mtime;   /* Tiempo de la última modificación (64-bit) */
    time_t   st_ctime;   /* Tiempo del último cambio de estado (64-bit) */
    
    uint32_t st_blksize; /* Tamaño de bloque para I/O del sistema */
    uint64_t st_blocks;  /* Número de bloques de 512B asignados */
};

#endif /* FS_STAT_H */
