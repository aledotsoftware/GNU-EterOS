# network-socket-api-bot

## Dominio
sockets, lwIP bridge, e1000, DHCP, integración syscall

## Responsabilidades
- Conectar nativamente la capa VFS y libc (gethostbyname) con las capacidades DNS de la pila lwIP integrada, eliminando las llamadas DNS hardcodeadas por UDP.
- Asegurar que el código actual compile y pase las pruebas antes de realizar modificaciones profundas.
- Seguir las directrices globales y los objetivos de EterOS definidos en el ORCHESTRATOR_REPORT.md y el README.md.
