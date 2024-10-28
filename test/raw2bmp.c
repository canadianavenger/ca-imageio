#include <stdio.h>
#include <image.h>
#include <bmp.h>
#include <utils.h>
#include "rawio.h"

int main(int argc, char *argv[]) {
    int rval = -1;
    pal_image_t *img = NULL;

    printf("ca-imageio RAW to BMP test\n");

    if(argc < 2) {
        printf("Error: Filename required\n");
        printf("USAGE: %s [filename]\n", filename(argv[0]));
        return -1;
    }

    if(NULL == (img = load_raw(argv[1]))) {
        printf("Unable to open '%s'\n", argv[1]);
        goto CLEANUP;
    }

    printf("Image is: %dx%d (%d colours)\n", img->width, img->height, img->colours);

    rval = save_bmp("OUT.BMP", img);
    if(0 != rval) {
        printf("Error saving RAW image\n");
        goto CLEANUP;
    }

    printf("Done\n");

    rval = 0;
CLEANUP:
    image_free(img);
    return rval;
}