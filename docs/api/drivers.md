# eterOS Driver API Guidelines

Drivers in eterOS are typically implemented as modules that expose a VFS node (e.g., in `/dev/`) for user-space interaction.

## Device Registration

Drivers register themselves by mounting a node into the Virtual File System or integrating with the `devfs`.

### Example: Character Device

To expose a character device (like a serial port or a custom sensor):

1. Define the necessary filesystem operations (`read`, `write`, `ioctl`).
2. Create a `fs_node_t` structure.
3. Set the `flags` to `FS_CHARDEVICE`.
4. Assign the function pointers.
5. Register the node in `devfs` (typically done in `devfs_init()` or dynamically if supported).

```c
static ssize_t my_device_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    // ... read logic ...
    return bytes_read;
}

// In initialization:
fs_node_t *my_node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
memset(my_node, 0, sizeof(fs_node_t));
strlcpy(my_node->name, "mydev", sizeof(my_node->name));
my_node->flags = FS_CHARDEVICE;
my_node->read = my_device_read;
// ... register node ...
```

## Framebuffer/DRM Drivers

Graphics drivers in eterOS provide a linear framebuffer accessible via `/dev/fb0` or `/dev/dri/card0` (DRM abstraction). User-space applications (like `eterland.c`) use `mmap` on these nodes to directly write pixel data to video memory.

## Network Drivers

Network drivers (like the `e1000`) integrate directly with the lwIP network stack rather than exposing raw packet interfaces via VFS. They implement lwIP's `netif` interface, providing `linkoutput` functions and calling `tcpip_input` (or `ethernet_input` depending on configuration) when packets are received.
