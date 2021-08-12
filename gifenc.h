#ifndef GIFENC_H
#define GIFENC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ge_GIF {
    uint16_t w, h;
    int depth;
    int transparent_index;
    int fd;
    int offset;
    int nframes;
    uint8_t *frame, *back;
    uint32_t partial;

    /* transparency only */
    bool has_unencoded_frame;
    uint16_t unencoded_delay;
    uint16_t unencoded_x,unencoded_y, unencoded_w, unencoded_h;


    uint8_t buffer[0xFF];
} ge_GIF;

ge_GIF *ge_new_gif(
    const char *fname, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int loop, int transparent_index
);
void ge_add_frame(ge_GIF *gif, uint16_t delay);
void ge_close_gif(ge_GIF* gif);

#ifdef __cplusplus
}
#endif
#endif /* GIFENC_H */
