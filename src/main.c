#include <drm_fourcc.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "util.h"
#include "draw.h"

static uint32_t find_crtc(int drm_fd, drmModeRes *res, drmModeConnector *conn,
        uint32_t *taken_crtcs)
{
    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder *enc = drmModeGetEncoder(drm_fd, conn->encoders[i]);
        if (!enc)
            continue;

        for (int i = 0; i < res->count_crtcs; ++i) {
            uint32_t bit = 1 << i;
            // Not compatible
            if ((enc->possible_crtcs & bit) == 0)
                continue;

            // Already taken
            if (*taken_crtcs & bit)
                continue;

            drmModeFreeEncoder(enc);
            *taken_crtcs |= bit;
            return res->crtcs[i];
        }

        drmModeFreeEncoder(enc);
    }

    return 0;
}

bool create_fb(int drm_fd, uint32_t width, uint32_t height, struct dumb_framebuffer *fb)
{
    int ret;

    struct drm_mode_create_dumb create = {
        .width = width,
        .height = height,
        .bpp = 32,
    };

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret < 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB");
        return false;
    }

    fb->height = height;
    fb->width = width;
    fb->stride = create.pitch;
    fb->handle = create.handle;
    fb->size = create.size;

    uint32_t handles[4] = { fb->handle };
    uint32_t strides[4] = { fb->stride };
    uint32_t offsets[4] = { 0 };

    ret = drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888,
            handles, strides, offsets, &fb->id, 0);
    if (ret < 0) {
        perror("drmModeAddFB2");
        goto error_dumb;
    }

    struct drm_mode_map_dumb map = { .handle = fb->handle };
    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (ret < 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB");
        goto error_fb;
    }

    fb->data = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
            drm_fd, map.offset);
    if (!fb->data) {
        perror("mmap");
        goto error_fb;
    }

    memset(fb->data, 0xff, fb->size);

    return true;

error_fb:
    drmModeRmFB(drm_fd, fb->id);
error_dumb:
    ;
    struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    return false;
}

static void destroy_fb(int drm_fd, struct dumb_framebuffer *fb)
{
	munmap(fb->data, fb->size);
	drmModeRmFB(drm_fd, fb->id);
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

int main(void)
{
    /* We just take the first GPU that exists. */
    int drm_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
    if (drm_fd < 0) {
        perror("/dev/dri/card0");
        return 1;
    }

    drmModeRes *res = drmModeGetResources(drm_fd);
    if (!res) {
        perror("drmModeGetResources");
        return 1;
    }

    struct connector *conn_list = NULL;
    uint32_t taken_crtcs = 0;

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector *drm_conn = drmModeGetConnector(drm_fd, res->connectors[i]);
        if (!drm_conn) {
            perror("drmModeGetConnector");
            continue;
        }

        struct connector *conn = malloc(sizeof *conn);
        if (!conn) {
            perror("malloc");
            goto cleanup;
        }

        conn->id = drm_conn->connector_id;
        snprintf(conn->name, sizeof conn->name, "%s-%"PRIu32,
                conn_str(drm_conn->connector_type),
                drm_conn->connector_type_id);
        conn->connected = drm_conn->connection == DRM_MODE_CONNECTED;

        conn->next = conn_list;
        conn_list = conn;

        printf("Found display %s\n", conn->name);

        if (!conn->connected) {
            printf("  Disconnected\n");
            goto cleanup;
        }

        if (drm_conn->count_modes == 0) {
            printf("No valid modes\n");
            conn->connected = false;
            goto cleanup;
        }

        conn->crtc_id = find_crtc(drm_fd, res, drm_conn, &taken_crtcs);
        if (!conn->crtc_id) {
            fprintf(stderr, "Could not find CRTC for %s\n", conn->name);
            conn->connected = false;
            goto cleanup;
        }

        printf("  Using CRTC %"PRIu32"\n", conn->crtc_id);

        // [0] is the best mode, so we'll just use that.
        conn->mode = drm_conn->modes[0];

        conn->width = conn->mode.hdisplay;
        conn->height = conn->mode.vdisplay;
        conn->rate = refresh_rate(&conn->mode);

        printf("  Using mode %"PRIu32"x%"PRIu32"@%"PRIu32"\n",
                conn->width, conn->height, conn->rate);

	if (!create_fb(drm_fd, conn->width, conn->height, &conn->fb[0])) {
		conn->connected = false;
		goto cleanup;
	}

	printf("  Created frambuffer with ID %"PRIu32"\n", conn->fb[0].id);

	if (!create_fb(drm_fd, conn->width, conn->height, &conn->fb[1])) {
		destroy_fb(drm_fd, &conn->fb[0]);
		conn->connected = false;
		goto cleanup;
	}

	printf("  Created frambuffer with ID %"PRIu32"\n", conn->fb[1].id);

	conn->front = &conn->fb[0];
	conn->back = &conn->fb[1];

        // Save the previous CRTC configuration
        conn->saved = drmModeGetCrtc(drm_fd, conn->crtc_id);

        // Perform the modeset
        int ret = drmModeSetCrtc(drm_fd, conn->crtc_id, conn->front->id, 0, 0,
                &conn->id, 1, &conn->mode);
        if (ret < 0) {
            perror("drmModeSetCrtc");
        }

cleanup:
        drmModeFreeConnector(drm_conn);
    }

    drmModeFreeResources(res);

    //draw_fb_coulours(conn_list);
    draw_fb_image(drm_fd, conn_list);

    // Cleanup
    struct connector *conn = conn_list;
    while (conn) {
        if (conn->connected) {
            // Cleanup framebuffer
		destroy_fb(drm_fd, &conn->fb[0]);
		destroy_fb(drm_fd, &conn->fb[1]);

            // Restore the old CRTC
            drmModeCrtc *crtc = conn->saved;
            if (crtc) {
                drmModeSetCrtc(drm_fd, crtc->crtc_id, crtc->buffer_id,
                        crtc->x, crtc->y, &conn->id, 1, &crtc->mode);
                drmModeFreeCrtc(crtc);
            }
        }

        struct connector *tmp = conn->next;
        free(conn);
        conn = tmp;
    }

    close(drm_fd);
}
