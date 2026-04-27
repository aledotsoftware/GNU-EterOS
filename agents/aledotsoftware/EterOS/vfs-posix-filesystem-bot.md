# vfs-posix-filesystem-bot

## Dominio
VFS, initrd, procfs, shmfs, FAT32/JFS, ELF loader

## Responsabilidades
- Implementar el puente para persistir el Journaling File System (JFS) volcando su estado hacia un disco físico subyacente mediante la capa bcache. Mantener initrd, devfs, procfs y el cargador ELF.
- Asegurar que el código actual compile y pase las pruebas antes de realizar modificaciones profundas.
- Seguir las directrices globales y los objetivos de EterOS definidos en el ORCHESTRATOR_REPORT.md y el README.md.
