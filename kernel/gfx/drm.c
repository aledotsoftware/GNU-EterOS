#include <gfx/drm.h>
#include <framebuffer.h>
#include <pmm.h>
#include <mm.h>
#include <vmm.h>
#include <string.h>
#include <errno.h>
#include <serial.h>

/* Mock/Stub DRM implementation for basic framebuffer compatibility */
/* Simulates a single CRTC, single Connector, single FB setup */

#define MOCK_CRTC_ID       1
#define MOCK_CONNECTOR_ID  2
#define MOCK_FB_ID         3
#define MOCK_HANDLE        4

static int drm_ioctl_getresources(struct drm_mode_card_res *res) {
    if (!res) return -EINVAL;

    if (res->count_fbs > 0 && res->fb_id_ptr) {
        uint32_t fb_id = MOCK_FB_ID;
        memcpy((void*)res->fb_id_ptr, &fb_id, sizeof(uint32_t));
    }
    if (res->count_crtcs > 0 && res->crtc_id_ptr) {
        uint32_t crtc_id = MOCK_CRTC_ID;
        memcpy((void*)res->crtc_id_ptr, &crtc_id, sizeof(uint32_t));
    }
    if (res->count_connectors > 0 && res->connector_id_ptr) {
        uint32_t conn_id = MOCK_CONNECTOR_ID;
        memcpy((void*)res->connector_id_ptr, &conn_id, sizeof(uint32_t));
    }

    res->count_fbs = 1;
    res->count_crtcs = 1;
    res->count_connectors = 1;
    res->count_encoders = 0;
    res->min_width = 320;
    res->max_width = 4096;
    res->min_height = 200;
    res->max_height = 4096;

    return 0;
}

static int drm_ioctl_create_dumb(struct drm_mode_create_dumb *dumb) {
    if (!dumb) return -EINVAL;

    /* If they ask for 0, use current FB dims */
    if (dumb->width == 0) dumb->width = framebuffer_get_width();
    if (dumb->height == 0) dumb->height = framebuffer_get_height();
    if (dumb->bpp == 0) dumb->bpp = framebuffer_get_bpp();

    dumb->pitch = dumb->width * (dumb->bpp / 8);
    dumb->size = dumb->pitch * dumb->height;
    dumb->handle = MOCK_HANDLE;

    return 0;
}

static int drm_ioctl_map_dumb(struct drm_mode_map_dumb *map) {
    if (!map) return -EINVAL;

    if (map->handle != MOCK_HANDLE) return -EINVAL;

    /* In a real DRM we'd return a fake offset that mmap intercepts.
       For EterOS, we can just return the physical address or 0 for now. */
    map->offset = 0;
    return 0;
}

static int dev_dri_card0_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    switch (request) {
        case DRM_IOCTL_MODE_GETRESOURCES:
            return drm_ioctl_getresources((struct drm_mode_card_res*)arg);
        case DRM_IOCTL_MODE_CREATE_DUMB:
            return drm_ioctl_create_dumb((struct drm_mode_create_dumb*)arg);
        case DRM_IOCTL_MODE_MAP_DUMB:
            return drm_ioctl_map_dumb((struct drm_mode_map_dumb*)arg);
        default:
            serial_write_string("[DRM] Unhandled IOCTL\n");
            return -EINVAL;
    }
}

/* Provide a direct mmap implementation for the DRM node if vfs supports it */
/* This will map the framebuffer memory */



int devfs_dri_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    if (index == 0) {
        strlcpy(entry->name, "card0", sizeof(entry->name));
        entry->inode = 20; /* Arbitrary */
        return 0;
    }
    return 1; /* EOF */
}

fs_node_t *devfs_dri_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    if (strcmp(name, "card0") == 0) {
        fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (!fnode) return 0;
        memset(fnode, 0, sizeof(fs_node_t));
        fnode->ref_count = 1;
        fnode->flags = FS_CHARDEVICE;
        fnode->mask = 0666;
        strlcpy(fnode->name, "card0", sizeof(fnode->name));
        fnode->ioctl = dev_dri_card0_ioctl;
        /* Custom mmap not strictly in fs_node_t standard, but let's see if we can just rely on ioctl for now */
        fnode->inode = 20;
        return fnode;
    }
    return 0;
}

void drm_init(void) {
    serial_write_string("[DRM] Subsystem initialized\n");
}
