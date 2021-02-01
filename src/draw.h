#pragma once

#include "util.h"

void draw_fb_coulours(int drm_fd, struct connector *conn_list);
void draw_fb_image(int drm_fd, struct connector *conn_list);
