#include <stdio.h>
#include <image.h>
#include <tga.h>
#include <utils.h>
#include "rawio.h"

int main(int argc, char *argv[]) {
    int rval = -1;
    pal_image_t *img = NULL;

    printf("ca-imageio RAW to TGA test\n");

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

    rval = save_tga("OUT.TGA", img);
    if(0 != rval) {
        printf("Error saving TGA image\n");
        goto CLEANUP;
    }

    printf("Done\n");

    rval = 0;
CLEANUP:
    image_free(img);
    return rval;
}