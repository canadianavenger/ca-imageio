#include <stdio.h>
#include <string.h>
#include "tga_priv.h"
#include <tga.h>
#include <stdbool.h>
#include <errno.h>

pal_image_t *load_tga(const char *fn) {
    int rval = 0;
    pal_image_t *img = NULL;
    FILE *fp = NULL;
    tga_palette_entry_t *pal = NULL;

    if(NULL == fn) {
        rval = EBADF;
        goto CLEANUP;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = errno;  // can't open input file
        goto CLEANUP;
    }

    // read the footer from the end of the file to check the signature
    tga_footer_t tgaf;
    memset(&tgaf, 0, sizeof(tga_footer_t));

    fseek(fp, -sizeof(tga_footer_t), SEEK_END);
    int nr = fread(&tgaf, sizeof(tga_footer_t), 1, fp);
    if(1 != nr) {
        rval = errno;
        goto CLEANUP;
    }

    if(0 != strncmp(tgaf.sig, TGA_SIG, 18)) {
    printf("TGA Signature not found\n");
        rval = EINVAL;
        goto CLEANUP;
    }

    // go back to the start of the file and read in the header
    tga_header_t tga;
    memset(&tga, 0, sizeof(tga_header_t));

    fseek(fp, 0, SEEK_SET);
    nr = fread(&tga, sizeof(tga_header_t), 1, fp);
    if(1 != nr) {
        rval = errno;
        goto CLEANUP;
    }

    printf("Valid TGA So far\n");
    printf("cmap type: %d\n", tga.colour_map_type);
    printf("image type: %d\n", tga.image_type);
    printf("Image: %dx%d (%d colours)\n", tga.image.width, tga.image.height, tga.cmap.colour_map_length);

    if((1 != tga.colour_map_type) || (1 != tga.image_type)) {
    printf("Invalid Colourmap or image type\n");
        rval = ENOTSUP;
        goto CLEANUP;
    }

    if(256 < (tga.cmap.colour_map_start + tga.cmap.colour_map_length)) {
    printf("Invalid Colourmap range\n");
        rval = ENOTSUP;
        goto CLEANUP;
    }

    if(24 != tga.cmap.colour_map_depth) {
    printf("Invalid Colourmap depth\n");
        rval = ENOTSUP;
        goto CLEANUP;
    }

    if(NULL == (img = image_alloc(tga.image.width, tga.image.height, tga.cmap.colour_map_start + tga.cmap.colour_map_length))) {
        rval = errno;
        goto CLEANUP;
    }

    // seek past any additional id data that may be after the header
    fseek(fp, sizeof(tga_header_t) + tga.id_length, SEEK_SET);

printf("PAL Start: %06zx\n", ftell(fp));

    // read in the palette
    int pal_entry_size = sizeof(tga_rgb_palette_entry_t);
    if(NULL == (pal = calloc(pal_entry_size, tga.cmap.colour_map_length))) {
        rval = errno;
        goto CLEANUP;
    }

    // read the palette
    nr = fread(pal, pal_entry_size, tga.cmap.colour_map_length, fp);
    if(nr != tga.cmap.colour_map_length) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    tga_rgb_palette_entry_t *ipal = &pal->rgb;
    for(int i = 0; i < tga.cmap.colour_map_length; i++) {
        img->pal[tga.cmap.colour_map_start + i].r = ipal[i].r;
        img->pal[tga.cmap.colour_map_start + i].g = ipal[i].g;
        img->pal[tga.cmap.colour_map_start + i].b = ipal[i].b;
    }

printf("IMG Start: %06zx\n", ftell(fp));

    // read the image
    nr = fread(img->data, img->width, img->height, fp);
    if(nr != img->height) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    free_s(pal);
    fclose_s(fp);
    return img;
CLEANUP:
    fclose_s(fp);
    image_free(img);
    free_s(pal);
    errno = rval;
    return NULL;
}