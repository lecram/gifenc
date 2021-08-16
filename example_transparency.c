#include <stdio.h>
#include <string.h>
#include "gifenc.h"

#define MIN(a,b) (((a)<(b))?(a):(b))


static void
add_frame(ge_GIF* gif, uint16_t delay)
{
	static int frameindex = 0;
    int height = 100;
	int width  = 100;

	printf("frame %03d: \n", frameindex++);
	for(int i =0; i < height; i++) {
		for(int j =0; j < width; j++) {
			printf("%c", gif->frame[i*width + j] ? 'Z' : ' ');
		}
		printf("\n");
	}

	printf("\n");
	ge_add_frame(gif, delay);
}

int
main(int argc, char const *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <file.gif> <disposal>\n", argv[0]);
		return 1;
	}

	size_t i, j;
	ge_GIF* gif;
	gif = ge_new_gif(
		argv[1], 100, 100,
		(uint8_t []) { /* R, G, B */
			0xff, 0xff, 0xff,
			0xda, 0x09, 0xff,
		},
		1,
		0, /* infinite loop */
		0 /* transparent background */
	);

	for (size_t t = 0; t < 100; t++) {
		/* clear the frame */
        memset(gif->frame, 0, (100*100));

		/* add the giant rectange to the frame on the left */
		for (i = 0; i < 100; i++)
			for (j = 0; j < 100; j++)
				gif->frame[i*100 + j] = i > 10 && i < 90 && j > 10 && j < 50;

		/* add the varying size right bar */
		for (i = 50; i > 0; i--)
			for (j = 60; j < 65; j++)
				gif->frame[i*100 + j] = i > MIN(t, 100 - t);
		add_frame(gif, 5);		
	}

	ge_close_gif(gif);
	return 0;
}