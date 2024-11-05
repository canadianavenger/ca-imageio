# ca-imageio
Basic read/write of some standard paletted graphics file formats for use with my various projects. This repository is meant to be included as a git module and linked into other projects. However if built as a stand-alone project the test code will be generated. Note: `ca-imageio` depends on [`ca-image`](https://github.com/canadianavenger/ca-image), though it is not expressly included as a module here to avoid code replication when included in a project.

## Library contents
- `include/image_bmp.h`: types, macros, and function declarations for saving and loading Windows BMP formatted images
  - `src/bmp/bmp_load.c`:  code for loading 4 and 8 bit BMP images (16 and 256 colour paletted)
  - `src/bmp/bmp_save.c`: code for saving 4 and 8 bit BMP images (16 and 256 colour paletted)
  - `src/bmp/bmp_priv.h`: private header containing the BMP specific structures and defines
- `include/image_png.h`: types, macros, and function declarations for saving and loading PNG formatted images
  - `src/png/png_load.c`:  code for loading paletted PNG images (up to 256 colour paletteted)
  - `src/png/png_save.c`: code for saving paletted PNG images (up to 256 colour paletteted)
  - `src/png/png_priv.h`: private header containing the PNG specific structures and defines
- `include/image_tga.h`: types, macros, and function declarations for saving and loading Truevision TGA formatted images
  - `src/tga/tga_load.c`:  code for loading paletted TGA images (up to 256 colour, not-compressed)
  - `src/tga/tga_save.c`: code for saving paletted TGA images (up to 256 colour, not-compressed)
  - `src/tga/tga_priv.h`: private header containing the TGA specific structures and defines

**Note**: PNG support is by way of [libpng](http://www.libpng.org), which also depends on [zlib](http://www.zlib.net/). Both of these libraries must be installed to build with PNG support, otherwise the library will not include PNG support. (if linking to a binary version of this library built with PNG support, libpng and zlib are not required)

### Test Code
- `test/rawio.h`: types, macros, and function declarations for saving and loading raw ca-image images
  - `test/rawio.c`: read/write code for the raw images
- `test/bmp2raw.c`: code for testing the BMP read code
- `test/raw2bmp.c`: code for testing the BMP save code
- `test/png2raw.c`: code for testing the PNG read code
- `test/raw2png.c`: code for testing the PNG save code
- `test/tga2raw.c`: code for testing the TGA read code
- `test/raw2tga.c`: code for testing the TGA save code

## Adding ca-imageio to a project
To add this submodule into a folder perform the following command:

`git submodule add https://github.com/canadianavenger/ca-imageio.git <folder name>`

if `git` does not automatically include the contents into `<folder name>` perfrom the following

`git submodule update --init --recursive`

For more info: [Working with Submodules on GitHub](https://github.blog/open-source/git/working-with-submodules/)

