#define STB_IMAGE_IMPLEMENTATION

#include <time.h>

#include "util.h"
#include "stb_image.h"

void draw_fb_coulours(struct connector *conn_list)
{
    // Draw some colours for 5 seconds
    uint8_t colour[4] = {0x00, 0x00, 0xff, 0x00}; // B G R X
    int inc = 1, dec = 2;
    for (int i = 0; i < 60 * 5; ++i)
    {
        colour[inc] += 15;
        colour[dec] -= 15;

        if (colour[dec] == 0)
        {
            dec = inc;
            inc = (inc + 2) % 3;
        }

        for (struct connector *conn = conn_list; conn; conn = conn->next)
        {
            if (!conn->connected)
                continue;

            struct dumb_framebuffer *fb = &conn->fb;

            for (uint32_t y = 0; y < fb->height; ++y)
            {
                uint8_t *row = fb->data + fb->stride * y;

                for (uint32_t x = 0; x < fb->width; ++x)
                {
                    row[x * 4 + 0] = colour[0];
                    row[x * 4 + 1] = colour[1];
                    row[x * 4 + 2] = colour[2];
                    row[x * 4 + 3] = colour[3];
                }
            }
        }

        // Should give 60 FPS
        struct timespec ts = {.tv_nsec = 16666667};
        nanosleep(&ts, NULL);
    }
}

void draw_fb_image(struct connector *conn_list)
{
    int img_width, img_height, channels;
    unsigned char *img = stbi_load("sky.jpg", &img_width, &img_height, &channels, 3);

    for (struct connector *conn = conn_list; conn; conn = conn->next)
    {
        if (!conn->connected)
            continue;

        struct dumb_framebuffer *fb = &conn->fb;

        for (uint32_t y = 0; y < fb->height; ++y)
        {
            uint8_t *row = fb->data + fb->stride * y;

            for (uint32_t x = 0; x < fb->width; ++x)
            {
                uint8_t img_x = x * img_width / fb->width;
                uint8_t img_y = y * img_height / fb->height;
                printf("imgx : %d, img_y: %d\n", img_x, img_y);
                printf("x : %d, y: %d\n", x, y);
                unsigned char *pixel_offset = img + (x + img_width * y) * 3;

                // B G R X
                row[x * 4 + 0] = pixel_offset[2];
                row[x * 4 + 1] = pixel_offset[1];
                row[x * 4 + 2] = pixel_offset[0];
                row[x * 4 + 3] = 0xFF;

                // FIXME: SEGV here... Maybe wrong x/y handling
            }
        }
    }
    // Wait 10 sec  
    struct timespec ts = {.tv_sec = 10, .tv_nsec = 0};
    nanosleep(&ts, NULL);
}