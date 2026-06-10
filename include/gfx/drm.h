#ifndef ETEROS_DRM_H
#define ETEROS_DRM_H

#include <types.h>
#include <ioctl.h>
#include <fs/vfs.h>

/* Minimal DRM IOCTLs (based on Linux DRM) */
#define DRM_IOCTL_BASE      'd'
#define DRM_COMMAND_BASE    0x40

/* Basic DRM structures */
struct drm_mode_card_res {
    uint32_t *fb_id_ptr;
    uint32_t *crtc_id_ptr;
    uint32_t *connector_id_ptr;
    uint32_t *encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
};

#define DRM_DISPLAY_MODE_LEN 32

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[DRM_DISPLAY_MODE_LEN];
};

struct drm_mode_crtc {
    uint32_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_modes;
    uint32_t count_props;
    uint32_t count_encoders;
    uint32_t encoder_id;
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mm_width, mm_height;
    uint32_t subpixel;
    uint32_t pad;
};

struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

/* IOCTL Definitions */
#define DRM_IOCTL_MODE_GETRESOURCES \
    _IOWR(DRM_IOCTL_BASE, 0xA0, struct drm_mode_card_res)

#define DRM_IOCTL_MODE_GETCRTC \
    _IOWR(DRM_IOCTL_BASE, 0xA1, struct drm_mode_crtc)

#define DRM_IOCTL_MODE_SETCRTC \
    _IOWR(DRM_IOCTL_BASE, 0xA2, struct drm_mode_crtc)

#define DRM_IOCTL_MODE_GETENCODER \
    _IOWR(DRM_IOCTL_BASE, 0xA6, struct drm_mode_get_encoder)

#define DRM_IOCTL_MODE_GETCONNECTOR \
    _IOWR(DRM_IOCTL_BASE, 0xA7, struct drm_mode_get_connector)

#define DRM_IOCTL_MODE_ADDFB \
    _IOWR(DRM_IOCTL_BASE, 0xAE, struct drm_mode_fb_cmd)

#define DRM_IOCTL_MODE_CREATE_DUMB \
    _IOWR(DRM_IOCTL_BASE, 0xB2, struct drm_mode_create_dumb)

#define DRM_IOCTL_MODE_MAP_DUMB \
    _IOWR(DRM_IOCTL_BASE, 0xB3, struct drm_mode_map_dumb)

/* Generic definition macros if not provided by ioctl.h */
#ifndef _IOC_NONE
# define _IOC_NONE  0U
# define _IOC_WRITE 1U
# define _IOC_READ  2U

# define _IOC(dir,type,nr,size) \
    (((dir)  << 30) | \
     ((size) << 16) | \
     ((type) << 8) | \
     ((nr)   << 0))
# define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
#endif

/* Functions */
void drm_init(void);
fs_node_t* devfs_dri_finddir(fs_node_t *node, char *name);
int devfs_dri_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);

#endif /* ETEROS_DRM_H */
