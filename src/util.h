#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>


struct dumb_framebuffer {
    uint32_t id;     // DRM object ID
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t handle; // driver-specific handle
    uint64_t size;   // size of mapping

    uint8_t *data;   // mmapped data we can write to
};

struct connector {
    uint32_t id;
    char name[16];
    bool connected;

    drmModeCrtc *saved;

    uint32_t crtc_id;
    drmModeModeInfo mode;

    uint32_t width;
    uint32_t height;
    uint32_t rate;

    struct dumb_framebuffer fb;

    struct connector *next;
};

/*
 * Get the human-readable string from a DRM connector type.
 * This is compatible with Weston's connector naming.
 */
const char *conn_str(uint32_t conn_type);

/*
 * Calculate an accurate refresh rate from 'mode'.
 * The result is in mHz.
 */
int refresh_rate(drmModeModeInfo *mode);
