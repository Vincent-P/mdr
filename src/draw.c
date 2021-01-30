#define STB_IMAGE_IMPLEMENTATION

#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "stb_image.h"

void draw_fb_coulours(struct connector *conn_list)
{
    // Draw some colours for ~3 seconds
    uint8_t colour[4] = {0x00, 0x00, 0xff, 0x00}; // B G R X
    int inc = 1, dec = 2;
    for (int i = 0; i < 60 * 3; ++i)
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
    size_t slide_nb = 0;
    while (1)
    {
        int img_width, img_height, channels;

        char filename[12];
        sprintf(filename, "slides/NSE-%d.jpg", slide_nb);
        if (access(filename, F_OK) != 0) {
            printf("End of slides !");
            return;
        }

        unsigned char *img = stbi_load(filename, &img_width, &img_height, &channels, 3);

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
                    size_t img_x = x * ((double)img_width / fb->width);
                    size_t img_y = y * ((double)img_height / fb->height);

                    unsigned char *pixel_offset = img + (img_x + img_width * img_y) * 3;

                    row[x * 4 + 0] = pixel_offset[2]; // B
                    row[x * 4 + 1] = pixel_offset[1]; // G
                    row[x * 4 + 2] = pixel_offset[0]; // R
                    row[x * 4 + 3] = 0xFF;
                }
            }
        }
        char command;
        do
        {
            command = getchar();
        } while (command == '\n');
        
        switch (command)
        {
        case 'd': // draw colors
            draw_fb_coulours(conn_list);
            break;
        case 'q': // Quit
            return;
        case 'p': // Previous
            slide_nb--;
            break;
        case 'n': // Next
        default:
            slide_nb++;
            break;
        }
    }
}