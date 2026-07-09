#include <stdio.h>
#include <image.h>
#include <image_pcx.h>
#include <utils.h>
#include "rawio.h"
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int rval = -1;
    pal_image_t *img = NULL;

    printf("ca-imageio RAW to PCX test\n");

    if(argc < 2) {
        printf("Error: Filename required\n");
        printf("USAGE: %s [filename]\n", filename(argv[0]));
        return -1;
    }

    if(NULL == (img = load_raw(argv[1]))) {
        printf("Unable to open '%s'\n", argv[1]);
        goto CLEANUP;
    }

    printf("Image is: %dx%d (%d colours", img->width, img->height, img->colours);
    printf(")\n");

    rval = save_pcx("OUT.PCX", img);
    if(0 != rval) {
        printf("Error saving PCX image\n");
        printf("[ERROR] %d: %s\n", rval, strerror(rval));
        
        goto CLEANUP;
    }

    printf("Done\n");

    rval = 0;
CLEANUP:
    image_free(img);
    return rval;
}
