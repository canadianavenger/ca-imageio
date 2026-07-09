#include <stdio.h>
#include <string.h>
#include "pcx_priv.h"
#include <image_pcx.h>
#include <stdbool.h>
#include <errno.h>
#include <memstream.h>
#include <pal-tools.h>

static int pcx_rle_decode(memstream_buf_t *dst, memstream_buf_t *src);

pal_image_t *load_pcx(const char *fn) {
    int rval = 0;
    pal_image_t *img = NULL;
    FILE *fp = NULL;
    uint8_t *fbuf = NULL;

    if(NULL == fn) {
        rval = EBADF;
        goto CLEANUP;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = errno;  // can't open input file
        goto CLEANUP;
    }

    // get the size of the file
    fseek(fp, 0, SEEK_END);
    size_t fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // go back to the start of the file and read in the header
    pcx_header_t pcx;
    memset(&pcx, 0, sizeof(pcx_header_t));

    int nr = fread(&pcx, sizeof(pcx_header_t), 1, fp);
    if(1 != nr) {
        rval = errno;
        goto CLEANUP;
    }
    fsz -= sizeof(pcx_header_t);

    // sanity check the magic, and the encoding
    if((PCX_MAGIC != pcx.magic) || (PCX_RLE != pcx.encoding)) {
        rval = EBADF;
        goto CLEANUP;
    }

    // check the version, we only support version 5 as we want 256 colour paletted images
    if(PCX_V5 != pcx.version) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    // we only support 1 plane and 4 plane encodings currently
    if((pcx.num_planes != 1) && (pcx.num_planes != 4)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    int max_colours = (1UL << (pcx.bits_per_pixel * pcx.num_planes));

    // must be a 16 or 256 colour paletted image
    if((16 != max_colours) && (256 != max_colours)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    pcx_pal256_t pal_vga;
    memset(&pal_vga, 0, sizeof(pcx_pal256_t));

    // some notes suggest that even 16 colour or 8 colour images may have a palette at EOF for PCXV5
    // so may need to adjust this to check for palettes of varying lengths at the end. For now
    // we assume only a 256 coour palette would be present for 1 plane/8 bits per pixel mode
    if(256 == max_colours) { // try to read the palette
        size_t cpz = ftell(fp); // save our position
        fseek(fp, -sizeof(pcx_pal256_t), SEEK_END); // goto the last 769 bytes in the file
        int nr = fread(&pal_vga, sizeof(pcx_pal256_t), 1, fp); // read in the palette
        if(1 != nr) {
            rval = errno;
            goto CLEANUP;
        }

        // check to see that it is there
        if(PCX_PAL_MAGIC != pal_vga.marker) {
            rval = EINVAL;
            goto CLEANUP;
        }

        fseek(fp, cpz, SEEK_SET); // restore our position
        fsz -= sizeof(pcx_pal256_t);
    }

    // we allocate the image buffer with enough space to include any padding.
    // so we don't need an additional buffer while decoding
    // the padding btes will fall off to the end of the allocated space after decoding
    int img_width = pcx.x_end - pcx.x_start + 1;
    int img_height = pcx.y_end - pcx.y_start + 1;
    int ipsz = img_width * img_height; // pixel size

    int bpp = (pcx.bits_per_pixel * pcx.num_planes);
    int stride = pcx.bytes_per_line * pcx.num_planes;
    int ibsz = stride * img_height; // buffer size, including any padding

    // extra bytes will be the difference between the buffer size and the pixel size
    if(NULL == (img = image_alloc(img_width, img_height, max_colours, 0))) {
        rval = errno;
        goto CLEANUP;
    }

    // copy in the palette, no manipulation should be required as the palette should already be 24bit RGB
    // we caan get away with memcpy here because the PCX palette format is the same as our internal one
    if(256 == max_colours) {
        memcpy(img->pal, pal_vga.pal, 256 * sizeof(pcx_rgb_palette_entry_t));

    }

    if(16 == max_colours) {
        memcpy(img->pal, pcx.pal_ega, 16 * sizeof(pcx_rgb_palette_entry_t));
    }

    // allocate a buffer large enough for the RLE data in the file, and the decoded data
    if(NULL == (fbuf = calloc(1, fsz+ibsz))) {
        rval = errno;
        goto CLEANUP;
    }

    // read the compressed stream into the 2nd half of the buffer
    nr = fread(fbuf+ibsz, fsz, 1, fp);
    if(nr != 1) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    memstream_buf_t pcxbuf = {fsz, 0, fbuf+ibsz}; // rle data is in 2nd half
    memstream_buf_t imgbuf = {ibsz, 0, fbuf};     // decompressed image is in first half
    // memstream_buf_t imgbuf = {img->image_size + img->extra_size, 0, img->pixels};
    rval = pcx_rle_decode(&imgbuf, &pcxbuf);
    if(rval != 0) {
        goto CLEANUP;
    }

    // now we need to deplane, or unpack if necessary
    if(pcx.num_planes == 1) {
        if(pcx.bits_per_pixel == 8) {
            if(ipsz == ibsz) { // no padding bytes, we can jsut copy the image over
                memcpy(img->pixels, fbuf, ipsz);
            } else { // we have padding per line, so we need to only copy the image bytes on a per line basis
                int img_stride = img->width;
                int pcx_stride = pcx.bytes_per_line;
                uint8_t *dst = img->pixels;
                uint8_t *src = fbuf;
                for(int line = 0; line < img->height; line++) {
                    memcpy(dst, src, img_stride);
                    dst += img_stride;
                    src += pcx_stride;
                }
            }
        } else { // must be 4 bits/pixel
            int skip = (pcx.bytes_per_line - ((img->width + 1) / 2)); // number of padding bytes at end of line
            uint8_t *dst = img->pixels;
            uint8_t *src = fbuf;
            int h = img->height;
            int w = img->width;
            for(int y = 0; y < h; y++) {
                for(int x = 0; x < w; x+=2) {
                    uint8_t pix = *src++;
                    *dst++ = ((pix >> 4) & 0x0f);
                    if((x+1) < w) *dst++ = (pix & 0x0f); // do not write 2nd pixel if we are an odd width, and at the end
                }
                src += skip; // skip any padding bytes
            }
        }
    } else { // must be 4 planes, and therefore 1 bit per pixel/plane
        int pcx_stride = pcx.bytes_per_line;
        int pcx_line_stride = pcx.bytes_per_line * 4;
        uint8_t *dst = img->pixels;
        uint8_t *src = fbuf;
        int h = img->height;
        int w = img->width;
        for(int y = 0; y < h; y++) {
            uint8_t *p0 = src;
            uint8_t *p1 = p0 + pcx_stride;
            uint8_t *p2 = p1 + pcx_stride;
            uint8_t *p3 = p2 + pcx_stride;
            uint8_t mask = 0x80;
            for(int x = 0; x < w; x++) {
                uint8_t pix = 0;
                if((*p0) & mask) pix |= 0x01;
                if((*p1) & mask) pix |= 0x02;
                if((*p2) & mask) pix |= 0x04;
                if((*p3) & mask) pix |= 0x08;
                *dst++ = pix;
                mask >>= 1;
                if(0 == mask) { // we've consumed all the bits, advance to the next byte
                    mask = 0x80;
                    p0++;
                    p1++;
                    p2++;
                    p3++;
                }
            }
            src += pcx_line_stride; 
        }
    }

    free_s(fbuf);
    fclose_s(fp);
    return img;
CLEANUP:
    fclose_s(fp);
    free_s(fbuf);
    image_free(img);
    errno = rval;
    return NULL;
}

/// @brief PCX rle decoder
/// @param dst pointer to a memstream buffer for holding the decompressed result data
/// @param src pointer to a memstream buffer holding the RLE compressed source data 
/// @return 
static int pcx_rle_decode(memstream_buf_t *dst, memstream_buf_t *src) {

    uint8_t val;
    while(src->pos < src->len) {
        val = src->data[src->pos++];
        int len = 1;
        uint8_t col = val;
        if(0xc0 < val) {
            if(src->pos == src->len) return EFAULT; // input stream unexpectidly ran out
            col = src->data[src->pos++];
            len = val & 0x3f;
        }

        if((dst->pos + len) > dst->len) return ENOBUFS; // we ran out of space

        for(int i = 0; i < len; i++) {
            dst->data[dst->pos++] = col;
        }
    }
    if(dst->pos != dst->len) return EINVAL;
    return 0;
}
