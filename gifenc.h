#ifndef GIFENC_H
#define GIFENC_H

#include <stdint.h>

typedef struct GIF {
    uint16_t w, h;
    int depth;
    int fd;
    int offset;
    int nframes;
    uint8_t *frame, *back;
    uint32_t partial;
    uint8_t buffer[0xFF];
} GIF;

GIF *new_gif(
    const char *fname, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int loop
);
void add_frame(GIF *gif, uint16_t delay);
void close_gif(GIF* gif);

#endif /* GIFENC_H */
