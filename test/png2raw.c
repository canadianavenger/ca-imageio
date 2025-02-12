#include <stdio.h>
#include <image.h>
#include <image_png.h>
#include <utils.h>
#include "rawio.h"

int main(int argc, char *argv[]) {
    int rval = -1;
    pal_image_t *img = NULL;

    printf("ca-imageio PNG to RAW test\n");

    if(argc < 2) {
        printf("Error: Filename required\n");
        printf("USAGE: %s [filename]\n", filename(argv[0]));
        return -1;
    }

    if(NULL == (img = load_png(argv[1]))) {
        printf("Unable to open '%s'\n", argv[1]);
        goto CLEANUP;
    }

    printf("Image is: %dx%d (%d colours", img->width, img->height, img->colours);
    if(0 <= img->transparent) printf(" - transparent idx: %d", img->transparent);
    printf(")\n");

    rval = save_raw("OUT.BIN", img);
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
