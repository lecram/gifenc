#include "gifenc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helper to write a little-endian 16-bit number portably */
#define fwrite_num(file, n) fwrite((uint8_t []) {(n) & 0xFF, (n) >> 8}, 1, 2, (file))

static uint8_t vga[0x30] = {
    0x00, 0x00, 0x00,
    0xAA, 0x00, 0x00,
    0x00, 0xAA, 0x00,
    0xAA, 0x55, 0x00,
    0x00, 0x00, 0xAA,
    0xAA, 0x00, 0xAA,
    0x00, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA,
    0x55, 0x55, 0x55,
    0xFF, 0x55, 0x55,
    0x55, 0xFF, 0x55,
    0xFF, 0xFF, 0x55,
    0x55, 0x55, 0xFF,
    0xFF, 0x55, 0xFF,
    0x55, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF,
};

struct Node {
    uint16_t key;
    struct Node *children[];
};
typedef struct Node Node;

static Node *
new_node(uint16_t key, int degree)
{
    Node *node = calloc(1, sizeof(*node) + degree * sizeof(Node *));
    if (node)
        node->key = key;
    return node;
}

static Node *
new_trie(int degree, int *nkeys)
{
    Node *root = new_node(0, degree);
    /* Create nodes for single pixels. */
    for (*nkeys = 0; *nkeys < degree; (*nkeys)++)
        root->children[*nkeys] = new_node(*nkeys, degree);
    *nkeys += 2; /* skip clear code and stop code */
    return root;
}

static void
del_trie(Node *root, int degree)
{
    if (!root)
        return;
    for (int i = 0; i < degree; i++)
        del_trie(root->children[i], degree);
    free(root);
}

#define fwrite_and_store(s, dst, file, src, n) \
do { \
    fwrite(src, 1, n, file); \
    if (s) { \
        memcpy(dst, src, n); \
        dst += n; \
    } \
} while (0);

static void put_loop(ge_GIF *gif, uint16_t loop);


ge_GIF *
ge_new_gif(
    const char *fname, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int bgindex, int loop
    )
{
    return ge_new_gif_filestream(fopen(fname, "wb"), width, height, palette, depth, bgindex, loop);
}

ge_GIF *
ge_new_gif_filestream(
    FILE * file, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int bgindex, int loop
)
{
    int i, r, g, b, v;
    int store_gct, custom_gct;
    int nbuffers = bgindex < 0 ? 2 : 1;
    ge_GIF *gif = calloc(1, sizeof(*gif) + nbuffers*width*height);
    if (!gif)
        goto no_gif;
    gif->w = width; gif->h = height;
    gif->bgindex = bgindex;
    gif->frame = (uint8_t *) &gif[1];
    gif->back = &gif->frame[width*height];
    gif->file = file;
    if (!gif->file)
        goto no_fd;
    fwrite("GIF89a", 1, 6, gif->file);
    fwrite_num(gif->file, width);
    fwrite_num(gif->file, height);
    store_gct = custom_gct = 0;
    if (palette) {
        if (depth < 0)
            store_gct = 1;
        else
            custom_gct = 1;
    }
    if (depth < 0)
        depth = -depth;
    gif->depth = depth > 1 ? depth : 2;
    fwrite((uint8_t []) {0xF0 | (depth-1), (uint8_t) bgindex, 0x00}, 1, 3, gif->file);
    if (custom_gct) {
        fwrite(palette, 1, 3 << depth, gif->file);
    } else if (depth <= 4) {
        fwrite_and_store(store_gct, palette, gif->file, vga, 3 << depth);
    } else {
        fwrite_and_store(store_gct, palette, gif->file, vga, sizeof(vga));
        i = 0x10;
        for (r = 0; r < 6; r++) {
            for (g = 0; g < 6; g++) {
                for (b = 0; b < 6; b++) {
                    fwrite_and_store(store_gct, palette, gif->file,
                      ((uint8_t []) {r*51, g*51, b*51}), 3
                    );
                    if (++i == 1 << depth)
                        goto done_gct;
                }
            }
        }
        for (i = 1; i <= 24; i++) {
            v = i * 0xFF / 25;
            fwrite_and_store(store_gct, palette, gif->file,
              ((uint8_t []) {v, v, v}), 3
            );
        }
    }
done_gct:
    if (loop >= 0 && loop <= 0xFFFF)
        put_loop(gif, (uint16_t) loop);
    return gif;
no_fd:
    free(gif);
no_gif:
    return NULL;
}

static void
put_loop(ge_GIF *gif, uint16_t loop)
{
    fwrite((uint8_t []) {'!', 0xFF, 0x0B}, 1, 3, gif->file);
    fwrite("NETSCAPE2.0", 1, 11, gif->file);
    fwrite((uint8_t []) {0x03, 0x01}, 1, 2, gif->file);
    fwrite_num(gif->file, loop);
    fwrite("\0", 1, 1, gif->file);
}

/* Add packed key to buffer, updating offset and partial.
 *   gif->offset holds position to put next *bit*
 *   gif->partial holds bits to include in next byte */
static void
put_key(ge_GIF *gif, uint16_t key, int key_size)
{
    int byte_offset, bit_offset, bits_to_write;
    byte_offset = gif->offset / 8;
    bit_offset = gif->offset % 8;
    gif->partial |= ((uint32_t) key) << bit_offset;
    bits_to_write = bit_offset + key_size;
    while (bits_to_write >= 8) {
        gif->buffer[byte_offset++] = gif->partial & 0xFF;
        if (byte_offset == 0xFF) {
            fwrite("\xFF", 1, 1, gif->file);
            fwrite(gif->buffer, 1, 0xFF, gif->file);
            byte_offset = 0;
        }
        gif->partial >>= 8;
        bits_to_write -= 8;
    }
    gif->offset = (gif->offset + key_size) % (0xFF * 8);
}

static void
end_key(ge_GIF *gif)
{
    int byte_offset;
    byte_offset = gif->offset / 8;
    if (gif->offset % 8)
        gif->buffer[byte_offset++] = gif->partial & 0xFF;
    if (byte_offset) {
        fwrite((uint8_t []) {byte_offset}, 1, 1, gif->file);
        fwrite(gif->buffer, 1, byte_offset, gif->file);
    }
    fwrite("\0", 1, 1, gif->file);
    gif->offset = gif->partial = 0;
}

static void
put_image(ge_GIF *gif, uint16_t w, uint16_t h, uint16_t x, uint16_t y)
{
    int nkeys, key_size, i, j;
    Node *node, *child, *root;
    int degree = 1 << gif->depth;

    fwrite(",", 1, 1, gif->file);
    fwrite_num(gif->file, x);
    fwrite_num(gif->file, y);
    fwrite_num(gif->file, w);
    fwrite_num(gif->file, h);
    fwrite((uint8_t []) {0x00, gif->depth}, 1, 2, gif->file);
    root = node = new_trie(degree, &nkeys);
    key_size = gif->depth + 1;
    put_key(gif, degree, key_size); /* clear code */
    for (i = y; i < y+h; i++) {
        for (j = x; j < x+w; j++) {
            uint8_t pixel = gif->frame[i*gif->w+j] & (degree - 1);
            child = node->children[pixel];
            if (child) {
                node = child;
            } else {
                put_key(gif, node->key, key_size);
                if (nkeys < 0x1000) {
                    if (nkeys == (1 << key_size))
                        key_size++;
                    node->children[pixel] = new_node(nkeys++, degree);
                } else {
                    put_key(gif, degree, key_size); /* clear code */
                    del_trie(root, degree);
                    root = node = new_trie(degree, &nkeys);
                    key_size = gif->depth + 1;
                }
                node = root->children[pixel];
            }
        }
    }
    put_key(gif, node->key, key_size);
    put_key(gif, degree + 1, key_size); /* stop code */
    end_key(gif);
    del_trie(root, degree);
}

static int
get_bbox(ge_GIF *gif, uint16_t *w, uint16_t *h, uint16_t *x, uint16_t *y)
{
    int i, j, k;
    int left, right, top, bottom;
    uint8_t back;
    left = gif->w; right = 0;
    top = gif->h; bottom = 0;
    k = 0;
    for (i = 0; i < gif->h; i++) {
        for (j = 0; j < gif->w; j++, k++) {
            back = gif->bgindex >= 0 ? gif->bgindex : gif->back[k];
            if (gif->frame[k] != back) {
                if (j < left)   left    = j;
                if (j > right)  right   = j;
                if (i < top)    top     = i;
                if (i > bottom) bottom  = i;
            }
        }
    }
    if (left != gif->w && top != gif->h) {
        *x = left; *y = top;
        *w = right - left + 1;
        *h = bottom - top + 1;
        return 1;
    } else {
        return 0;
    }
}

static void
add_graphics_control_extension(ge_GIF *gif, uint16_t d)
{
    uint8_t flags = ((gif->bgindex >= 0 ? 2 : 1) << 2) + 1;
    fwrite((uint8_t []) {'!', 0xF9, 0x04, flags}, 1, 4, gif->file);
    fwrite_num(gif->file, d);
    fwrite((uint8_t []) {(uint8_t) gif->bgindex, 0x00}, 1, 2, gif->file);
}

void
ge_add_frame(ge_GIF *gif, uint16_t delay)
{
    uint16_t w, h, x, y;
    uint8_t *tmp;

    if (delay || (gif->bgindex >= 0))
        add_graphics_control_extension(gif, delay);
    if (gif->nframes == 0) {
        w = gif->w;
        h = gif->h;
        x = y = 0;
    } else if (!get_bbox(gif, &w, &h, &x, &y)) {
        /* image's not changed; save one pixel just to add delay */
        w = h = 1;
        x = y = 0;
    }
    put_image(gif, w, h, x, y);
    gif->nframes++;
    if (gif->bgindex < 0) {
        tmp = gif->back;
        gif->back = gif->frame;
        gif->frame = tmp;
    }
}

size_t
ge_close_gif(ge_GIF* gif)
{
    fwrite(";", 1, 1, gif->file);
    size_t sz = ftell(gif->file);
    fclose(gif->file);
    free(gif);
    return sz;
}
