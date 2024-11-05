/*
 * rawio.h 
 * interface definitions for a writing a raw indexed image to disk, for testing
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#ifndef CA_IMG_RAW_IO
#define CA_IMG_RAW_IO

/// @brief saves the image pointed to by src as a BMP
/// @param fn name of the file to create and write to
/// @param src pointer to a basic_image_t structure containing the image
/// @return 0 on success, otherwise an error code
int save_raw(const char *fn, pal_image_t *src);

/// @brief loads the BMP image from a file
/// @param fn name of file to load
/// @return  pointer to a basic_image_t structure containing the image, or null on error (errno is set)
pal_image_t *load_raw(const char *fn);

#endif
