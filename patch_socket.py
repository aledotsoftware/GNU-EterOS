import sys

with open('kernel/net/socket.c', 'r') as f:
    content = f.read()

content = content.replace('#include "lwip/sockets.h"\n#include "lwip/api.h"', '#include "lwip/sockets.h"')

with open('kernel/net/socket.c', 'w') as f:
    f.write(content)
