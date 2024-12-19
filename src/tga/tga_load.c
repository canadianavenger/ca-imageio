#include <stdio.h>
#include <string.h>
#include "tga_priv.h"
#include <image_tga.h>
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

    // must be a paletteted image
    if((1 != tga.colour_map_type) || (1 != tga.image_type)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    // we only accept 8 bit palettes
    if(256 < (tga.cmap.colour_map_start + tga.cmap.colour_map_length)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    // we only accept 24 and 32 bit colour maps (RGB and ARGB)
    if((24 != tga.cmap.colour_map_depth) && (32 != tga.cmap.colour_map_depth)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    if(NULL == (img = image_alloc(tga.image.width, tga.image.height, tga.cmap.colour_map_start + tga.cmap.colour_map_length, 0))) {
        rval = errno;
        goto CLEANUP;
    }

    // seek past any additional id data that may be after the header
    fseek(fp, sizeof(tga_header_t) + tga.id_length, SEEK_SET);

    // allocate the palette buffer
    int pal_entry_size = tga.cmap.colour_map_depth / 8;
    if(NULL == (pal = calloc(tga.cmap.colour_map_length, pal_entry_size))) {
        rval = errno;
        goto CLEANUP;
    }

    // read the palette
    nr = fread(pal, pal_entry_size, tga.cmap.colour_map_length, fp);
    if(nr != tga.cmap.colour_map_length) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    if(3 == pal_entry_size) { // RGB data
        tga_rgb_palette_entry_t *ipal = &pal->rgb;
        for(int i = 0; i < tga.cmap.colour_map_length; i++) {
            img->pal[tga.cmap.colour_map_start + i].r = ipal[i].r;
            img->pal[tga.cmap.colour_map_start + i].g = ipal[i].g;
            img->pal[tga.cmap.colour_map_start + i].b = ipal[i].b;
        }
    } else { // ARGB data
        tga_argb_palette_entry_t *ipal = &pal->argb;
        int first_trans = tga.cmap.colour_map_length;
        for(int i = 0; i < tga.cmap.colour_map_length; i++) {
            img->pal[tga.cmap.colour_map_start + i].r = ipal[i].r;
            img->pal[tga.cmap.colour_map_start + i].g = ipal[i].g;
            img->pal[tga.cmap.colour_map_start + i].b = ipal[i].b;
            if((0 == ipal[i].a) && (i < first_trans)) first_trans = i; // capture the first transparent value
        }
        if(first_trans == tga.cmap.colour_map_length) { // no fully transparent entry found
            first_trans = -1;
        } else { // we have an index
            first_trans += tga.cmap.colour_map_start; // correct for the start offset
        }
        img->transparent = first_trans;
    }

    // read the image
    nr = fread(img->pixels, img->width, img->height, fp);
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
