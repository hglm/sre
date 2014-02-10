/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#ifdef __GNUC__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif
#include <png.h>

#include "sre.h"
#include "sre_internal.h"

static bool checked_texture_formats = false;
static int ETC1_internal_format = - 1;
static int ETC2_RGB8_internal_format = - 1;
static int DXT1_internal_format = - 1;
static int SRGB_DXT1_internal_format = - 1;
static int BPTC_internal_format = - 1;
static int SRGB_BPTC_internal_format = - 1;
static int BPTC_float_internal_format = - 1;
static float max_anisotropy, min_anisotropy;

// Standard textures for fall-back when texture doesn't load.
static sreTexture *standard_texture = NULL;
static sreTexture *standard_texture_wrap_repeat = NULL;

static void RegisterTexture(sreTexture *tex, int type);

static void CheckTextureFormats() {
   GLint num = 0;

//   GLEW_OES_compressed_ETC1_RGB8_texture = 0;

   glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num);
   if (num > 0) {
      GLint* formats = (GLint*)calloc(num, sizeof(GLint));

      glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);

      for (int index = 0; index < num; index++) {
//         printf("Checking format 0x%04X.\n", formats[index]);
#ifdef OPENGL_ES2
         if (formats[index] == GL_ETC1_RGB8_OES) {
             printf("ETC1 texture format supported.\n");
             ETC1_internal_format = GL_ETC1_RGB8_OES;
//            GLEW_OES_compressed_ETC1_RGB8_texture = 1;
         }
#endif
	if (formats[index] == 0x9274) {
		printf("ETC2_RGB8 texture format supported.\n");
		ETC2_RGB8_internal_format = 0x9274;
	}
#if defined(OPENGL)
         if (formats[index] == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
             printf("DXT1 texture format supported.\n");
             DXT1_internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
         }
         if (formats[index] == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT) {
             printf("SRGB DXT1 texture format supported.\n");
             SRGB_DXT1_internal_format = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
         }
#endif
      }

      free(formats);
   }

#ifndef OPENGL_ES2
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    printf("OpenGL version %d.%d.\n", major, minor);
    if (GLEW_ARB_texture_compression_bptc) {
        BPTC_internal_format = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
        SRGB_BPTC_internal_format = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
        BPTC_float_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
        printf("BPTC texture formats supported.\n");
    }
#endif

    min_anisotropy = 0;
    max_anisotropy = 0;
#ifndef OPENGL_ES2
    if (GLEW_EXT_texture_filter_anisotropic) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
        printf("Max anisotropy for anisotropic filtering: %.1f.\n",
            max_anisotropy);
    }
#endif

    checked_texture_formats = true;
}

sreTexture::sreTexture(int w, int h) {
    width = w;
    height = h;
    data = new unsigned int[w * h];
    bytes_per_pixel = 4;
    format = TEXTURE_FORMAT_RAW;
}

sreTexture::~sreTexture() {
    delete [] data;
}

void sreTexture::UploadGL(int type) {
#ifndef NO_SRGB
    if (format == TEXTURE_FORMAT_RAW) {
        // Regular (image) textures should handled as SRGB.
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB || type == TEXTURE_TYPE_WRAP_REPEAT)
            if (bytes_per_pixel == 4)
                format = TEXTURE_FORMAT_RAW_SRGBA8;
            else
                format = TEXTURE_FORMAT_RAW_SRGB8;
        else // Normal maps etc. are stored stored in linear color space format.
            if (bytes_per_pixel == 4)
                format = TEXTURE_FORMAT_RAW_RGBA8;
            else
                format = TEXTURE_FORMAT_RAW_RGB8;
    }
#endif
    GLuint texid;
    glGenTextures(1, &texid);
    opengl_id = texid;
    glBindTexture(GL_TEXTURE_2D, opengl_id);
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (width == (1 << i))
            count++;
        if (height == (1 << i))
            count++;
    }
// printf("count = %d, bytes_per_pixel = %d\n", count, bytes_per_pixel);
    bool generate_mipmaps = false;
    if (glGetError() != GL_NO_ERROR) {
        printf("Error before glTexParameteri.\n");
        exit(1);
    }
    if (count != 2 || format == TEXTURE_FORMAT_ETC1) {
        // Mip-maps not supported for textures that are not a power of two in width and height.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        generate_mipmaps = true;
    }
    if (type == TEXTURE_TYPE_WRAP_REPEAT) {
        if (count != 2) {
            printf("Error - repeating textures require power of two texture dimensions.\n");
            exit(1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (glGetError() != GL_NO_ERROR) {
        printf("Error after glTexParameteri.\n");
        exit(1);
    }
    GLint internal_format;
    if (format == TEXTURE_FORMAT_ETC1) {
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, ETC1_internal_format, width, height, 0, (width / 4) * (height / 4) * 8, data);
        internal_format = ETC1_internal_format;
    }
    else {
#ifdef OPENGL_ES2
        if (bytes_per_pixel == 4)
            internal_format = GL_RGBA;
        else
            internal_format = GL_RGB;
#else
        if (format == TEXTURE_FORMAT_RAW_RGBA8 || format == TEXTURE_FORMAT_RAW_RGB8)
            if (bytes_per_pixel == 4)
                internal_format = GL_RGBA8;
            else
                internal_format = GL_RGB8;
        else if (format == TEXTURE_FORMAT_RAW_SRGBA8 || format == TEXTURE_FORMAT_RAW_SRGB8) {
            if (bytes_per_pixel == 4)
                internal_format = GL_SRGB_ALPHA;
            else
                internal_format = GL_SRGB;
        }
        else {
            printf("Error -- unknown texture format.\n");
            exit(1);
        }
#endif
        if (bytes_per_pixel == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        else {
            /* Unpack alignment of 1. */
            GLint previousUnpackAlignment;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
        }
    }
    if (glGetError() != GL_NO_ERROR) {
        printf("Error after glTexImage2D (internal_format = 0x%04X)\n", internal_format);
        exit(1);
    }
    if (generate_mipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        printf("Error after glGenerateMipmap (internal format = %d).\n", internal_format);
        exit(1);
    }
    delete [] data;
 
    RegisterTexture(this, type);
}

void sreTexture::ConvertFrom24BitsTo32Bits() {
    int n = width * height;
    unsigned int *data2 = new unsigned int[n];
    unsigned char *data1 = (unsigned char *)data;
    for (int i = 0; i < n; i++) {
        data2[i] = data1[i * 3] + (data1[i * 3 + 1] << 8) + (data1[i * 3 + 2] << 16) + 0xFF000000;
    }
    delete [] data;
    data = data2;
    bytes_per_pixel = 4;
}

// PNG loading.

void sreTexture::LoadPNG(const char *filename, int type) {
    int png_width, png_height;
    png_byte color_type;
    png_byte bit_depth;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers;

        png_byte header[8];    // 8 is the maximum size that can be checked

        /* open file and test for it being a png */
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            printf("Error - File %s could not be opened for reading\n", filename);
            exit(1);
        }
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8)) {
            printf("Error - File %s is not recognized as a PNG file\n", filename);
            exit(1);
        }

        /* initialize stuff */
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            printf("png_create_read_struct failed\n");
            exit(1);
        }   

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            printf("png_create_info_struct failed\n");
            exit(1);
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            printf("Error during init_io");
            exit(1);
        }

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        png_width = png_get_image_width(png_ptr, info_ptr);
        png_height = png_get_image_height(png_ptr, info_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        int number_of_passes = png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        /* read file */
        if (setjmp(png_jmpbuf(png_ptr))) {
            printf("Error during read_image\n");
            exit(1);
        }

        row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * png_height);
        for (int y = 0; y < png_height; y++)
            row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));

        png_read_image(png_ptr, row_pointers);

        fclose(fp);

    width = png_width;
    height = png_height;
    printf("Loading texture with size (%d x %d), bit depth %d.\n", width, height, bit_depth);
    if (color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGBA && color_type != PNG_COLOR_TYPE_GRAY) {
        printf("Error - expected truecolor color format.\n");
        exit(1);
    }
    if (bit_depth != 8) {
        printf("Error - expected bit depth of 8 in PNG file (depth = %d).\n", bit_depth);
        exit(1);
    }
    if (color_type == PNG_COLOR_TYPE_RGB)
        bytes_per_pixel = 3;
    else
    if (color_type == PNG_COLOR_TYPE_RGBA)
        bytes_per_pixel = 4;
    else
        bytes_per_pixel = 3; // Greyscale.
    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (width > max_texture_size || height > max_texture_size) {
        printf("Error - texture size of (%d x %d) is too large (max supported %d x %d).\n",
            width, height, max_texture_size, max_texture_size);
        exit(1);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY) {
        data = (unsigned int *)new unsigned char[width * height * 3];
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                 *((unsigned char *)data + y * width * 3 + x * 3) = *((unsigned char *)row_pointers[y] + x);
                 *((unsigned char *)data + y * width * 3 + x * 3 + 1) = *((unsigned char *)row_pointers[y] + x);
                 *((unsigned char *)data + y * width * 3 + x * 3 + 2) = *((unsigned char *)row_pointers[y] + x);
            }
    }
    else
    if (bytes_per_pixel == 3) {
        data = (unsigned int *)new unsigned char[width * height * 3];
        for (int y = 0; y < height; y++)
            memcpy((unsigned char *)data + y * width * 3, row_pointers[y], width * bytes_per_pixel);
    }
    else {
        data = new unsigned int[width * height];
        for (int y = 0; y < height; y++)
            memcpy(data + y * width, row_pointers[y], width * bytes_per_pixel);
    }
    for (int y = 0; y < height; y++)
        free(row_pointers[y]);
    free(row_pointers);

    format = TEXTURE_FORMAT_RAW;
    if (type != TEXTURE_TYPE_WILL_MERGE_LATER) {
        UploadGL(type);
    }
}

#define KTX_HEADER_SIZE	(64)
/* KTX files require an unpack alignment of 4 */
#define KTX_GL_UNPACK_ALIGNMENT 4
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

typedef unsigned char khronos_uint8_t;
typedef unsigned int khronos_uint32_t;

/**
 * @internal
 * @brief KTX file header
 *
 * See the KTX specification for descriptions
 * 
 */
typedef struct KTX_header_t {
	khronos_uint8_t  identifier[12];
	khronos_uint32_t endianness;
	khronos_uint32_t glType;
	khronos_uint32_t glTypeSize;
	khronos_uint32_t glFormat;
	khronos_uint32_t glInternalFormat;
	khronos_uint32_t glBaseInternalFormat;
	khronos_uint32_t pixelWidth;
	khronos_uint32_t pixelHeight;
	khronos_uint32_t pixelDepth;
	khronos_uint32_t numberOfArrayElements;
	khronos_uint32_t numberOfFaces;
	khronos_uint32_t numberOfMipmapLevels;
	khronos_uint32_t bytesOfKeyValueData;
} KTX_header;

bool sreTexture::LoadKTX(const char *filename, int type) {
    FILE *f = fopen(filename, "rb");
    // Read header.
    KTX_header header;
    fread(&header, 1, KTX_HEADER_SIZE, f);
    if (header.endianness != 0x04030201) {
        printf("Endianness wrong way around in .ktx file.\n");
	for (int i = 12; i < sizeof(KTX_header); i += 4) {
            unsigned char *h = (unsigned char *)&header;
            unsigned char temp = h[i];
            h[i] = h[i + 3];
            h[i + 3] = temp;
            temp = h[i + 1];
            h[i + 1] = h[i + 2];
            h[i + 2] = temp;
	}
    }
    printf("Loading KTX texture with size (%d x %d), %d mipmap levels.\n", header.pixelWidth, header.pixelHeight,
        header.numberOfMipmapLevels);
    // Skip metadata
    unsigned char *metadata = (unsigned char *)alloca(header.bytesOfKeyValueData);
    fread(metadata, 1, header.bytesOfKeyValueData, f);
    GLenum glInternalFormat = header.glInternalFormat;
    int supported_format = - 1;
    switch (glInternalFormat) {
#ifdef OPENGL_ES2
    case GL_ETC1_RGB8_OES :
        if (ETC1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_ETC1;
        break;
#endif
    case 0x9274 :	// ETC2_RGB8
	if (ETC2_RGB8_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_ETC2_RGB8;
        break;
#ifndef OPENGL_ES2
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT :
#ifndef NO_SRGB
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB || type == TEXTURE_TYPE_WRAP_REPEAT) {
    	    if (SRGB_DXT1_internal_format != - 1) {
                supported_format = TEXTURE_FORMAT_SRGB_DXT1;
                glInternalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
            }
        }
        else
#endif
	if (DXT1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_DXT1;
        break;
#endif
#ifndef OPENGL_ES2
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT :
	if (SRGB_DXT1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_SRGB_DXT1;
        break;
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB :
#ifndef NO_SRGB
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB || type == TEXTURE_TYPE_WRAP_REPEAT) {
            if (SRGB_BPTC_internal_format != - 1) {
                supported_format = TEXTURE_FORMAT_SRGB_BPTC;
                glInternalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
            }
        }
        else
#endif
        if (BPTC_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_BPTC;
        break;
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB :
        if (SRGB_BPTC_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_SRGB_BPTC;
        break;
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB :
        if (BPTC_float_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_BPTC_FLOAT;
        break;
#endif
default :
        break;
    }
    if (supported_format == - 1) {
        printf("Texture format in KTX file not supported (glInternalFormat = 0x%04X).\n", glInternalFormat);
	return false;
    }

    /* KTX files require an unpack alignment of 4 */
    GLint previousUnpackAlignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    if (previousUnpackAlignment != KTX_GL_UNPACK_ALIGNMENT)
        glPixelStorei(GL_UNPACK_ALIGNMENT, KTX_GL_UNPACK_ALIGNMENT);
    // Generate the texture.
    GLuint texid;
    glGenTextures(1, &texid);
    opengl_id = texid;
    glBindTexture(GL_TEXTURE_2D, opengl_id);

    khronos_uint32_t faceLodSize;
    khronos_uint32_t faceLodSizeRounded;
    khronos_uint32_t level;
    khronos_uint32_t face;
    khronos_uint32_t dataSize = 0;

    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (header.pixelWidth == (1 << i))
            count++;
        if (header.pixelHeight == (1 << i))
            count++;
    }
    if (header.numberOfMipmapLevels == 1) {
        // Mip-maps not supported for textures that are not a power of two in width and height.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        if (count != 2) {
            printf("Error -- KTX compressed non-power-of-2 mipmapped textures not supported.\n");
            exit(1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    if (type == TEXTURE_TYPE_WRAP_REPEAT) {
        if (count != 2) {
            printf("Error - repeating textures require power of two texture dimensions.\n");
            exit(1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    data = NULL;
    for (level = 0; level < header.numberOfMipmapLevels; level++) {
        GLsizei pixelWidth  = MAX(1, header.pixelWidth  >> level);
        GLsizei pixelHeight = MAX(1, header.pixelHeight >> level);
        GLsizei pixelDepth  = MAX(1, header.pixelDepth  >> level);

        fread(&faceLodSize, 1, sizeof(khronos_uint32_t), f);
        faceLodSizeRounded = (faceLodSize + 3) & ~(khronos_uint32_t)3;
        if (!data) {
            /* allocate memory sufficient for the first level */
	    data = (unsigned int *)malloc(faceLodSizeRounded);
 	    dataSize = faceLodSizeRounded;
        }
        for (face = 0; face < header.numberOfFaces; ++face) {
            fread(data, 1, faceLodSizeRounded, f);
            if (header.numberOfArrayElements)
                pixelHeight = header.numberOfArrayElements;
            glCompressedTexImage2D(GL_TEXTURE_2D + face, level, glInternalFormat, pixelWidth, pixelHeight, 0,
                    faceLodSize, data);
            GLenum errorTmp = glGetError();
            if (errorTmp != GL_NO_ERROR) {
                printf("Error loading .ktx texture.\n");
                exit(1);
            }
        }
    }
    fclose(f);
    free(data);
    /* restore previous GL state */
    if (previousUnpackAlignment != KTX_GL_UNPACK_ALIGNMENT)
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
//    glGenerateMipmap(GL_TEXTURE2D);
    format = supported_format;

    RegisterTexture(this, type);
    return true;
}

void sreTexture::LoadDDS(const char *filename, int type) {
    unsigned char header[124];
 
    FILE *fp; 
    /* try to open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Error -- cannot access texture file %s.\n", filename);
        exit(1);
    }
 
    /* verify the type of file */
    char filecode[4];
    fread(filecode, 1, 4, fp);
    if (strncmp(filecode, "DDS ", 4) != 0) {
        fclose(fp);
        printf("Error -- .dds file is not a DDS file.\n");
        exit(1);
    }
 
    /* get the surface desc */
    fread(&header, 124, 1, fp);
 
    height      = *(unsigned int *)(header + 8);
    width         = *(unsigned int *)(header + 12);
    unsigned int linearSize     = *(unsigned int *)(header + 16);
    unsigned int mipMapCount = *(unsigned int *)(header + 24);
    unsigned int fourCC      = *(unsigned int *)(header + 80);

    printf("Loading DDS texture with size (%d x %d), %d mipmap levels.\n", width, height, mipMapCount);

    unsigned char * buffer;
    unsigned int bufsize;
    /* how big is it going to be including all mipmaps? */
    bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;
    buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
    fread(buffer, 1, bufsize, fp);
    /* close the file pointer */
    fclose(fp);

#if 0
    unsigned int components  = (fourCC == FOURCC_DXT1) ? 3 : 4;
    unsigned int _format;
    switch(fourCC)
    {
    case FOURCC_DXT1:
        _format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
    case FOURCC_DXT3:
        _format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
    case FOURCC_DXT5:
        _format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
    default:
        printf("DXT texture format not supported.\n");
        exit(1);
    }
    if (_format != GL_COMPRESSED_RGBA_S3TC_DX1_EXT) {
        printf("Error -- only DXT1 is supported.\n");
        exit(1);
    }
#endif

    if (glGetError() != GL_NO_ERROR) {
        printf("Error before loading DDS texture.\n");
        exit(1);
    }

    // Create one OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    opengl_id = textureID;
 
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Check whether the texture dimensions are a power of two.
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (width == (1 << i))
            count++;
        if (height == (1 << i))
            count++;
    }
    // Check for buggy dds files that have too many mipmap levels for non-square textures.
    int w = width;
    int h = height;
    for (int i = 0; i < mipMapCount; i++) {
        if (w == 0 || h == 0) {
            mipMapCount = i;
            break;
        }
        w /= 2;
        h /= 2;
    }
#ifndef OPENGL_ES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipMapCount - 1);
#endif
    if (mipMapCount == 1) {
        // Mip-maps not supported for textures that are not a power of two in width and height.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        if (count != 2) {
            printf("Error -- DXT1 compressed non-power-of-2 mipmapped textures not supported.\n");
            exit(1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    if (type == TEXTURE_TYPE_WRAP_REPEAT) {
        if (count != 2) {
            printf("Error - repeating textures require power of two texture dimensions.\n");
            exit(1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

//    unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    unsigned int blockSize = 8;
    unsigned int offset = 0;

    GLint internal_format;
#ifndef OPENGL_ES2
#ifdef NO_SRGB
        internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        format = TEXTURE_FORMAT_DXT1;
#else
    if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB || type == TEXTURE_TYPE_WRAP_REPEAT) {
        internal_format = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
        format = TEXTURE_FORMAT_SRGB_DXT1;
    }
    else {
        internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        format = TEXTURE_FORMAT_DXT1;
    }
#endif
#endif

    /* Load the mipmaps. */
    w = width;
    h = height;
    for (unsigned int level = 0; level < mipMapCount; ++level)
    {
        unsigned int size;
        size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, level, internal_format, w, h, 
            0, size, buffer + offset);
        GLenum errorTmp = glGetError();
        if (errorTmp != GL_NO_ERROR) {
            printf("Error loading .dds texture.\n");
            exit(1);
        }

        offset += size;
        w /= 2;
        h /= 2;
    }
    free(buffer); 
    RegisterTexture(this, type);
}

static bool FileExists(const char *filename) {
#ifdef __GNUC__
    struct stat stat_buf;
    int r = stat(filename, &stat_buf);
    if (r == - 1)
        return false;
    return true;
#else
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
	return false;
    fclose(f);
    return true;
#endif
}

sreTexture::sreTexture(const char *basefilename, int type) {
    if (!checked_texture_formats)
        CheckTextureFormats();
    char s[80];
    sprintf(s, "%s.ktx", basefilename);
    bool success = false;
    if (type != TEXTURE_TYPE_USE_RAW_TEXTURE && FileExists(s))
        success = LoadKTX(s, type);
    if (!success) {
        sprintf(s, "%s.dds", basefilename);
        if (DXT1_internal_format != -1 && type != TEXTURE_TYPE_USE_RAW_TEXTURE && FileExists(s)) {
            LoadDDS(s, type);
            success = 1;
        }
    }
    if (!success) {
            sprintf(s, "%s.png", basefilename);
            if (FileExists(s))
                LoadPNG(s, type);
            else {
                printf("Error -- texture file %s(.png, .ktx, .dds) not found or not supported.\n", basefilename);
                printf("Replacing with internal standard texture.\n");
                sreTexture *tex;
                if (type == TEXTURE_TYPE_WRAP_REPEAT)
                    tex = sreGetStandardTextureWrapRepeat();
                else
                    tex = sreGetStandardTexture();
                // Copy the fields of the standard texture, not graceful but it works.
                width = tex->width;
                height = tex->height;
                bytes_per_pixel = tex->bytes_per_pixel;
                format = tex->format;
                // Do not copy the data field because is invalid anyway (previously been freed after upload).
                data = NULL;
                opengl_id = tex->opengl_id;
            }
    }
}

void sreTexture::ChangeParameters(int flags, int filtering, float anisotropy) {
    glBindTexture(GL_TEXTURE_2D, opengl_id);
    if (flags & SRE_TEXTURE_FLAG_SET_FILTER)
        switch (filtering) {
        case SRE_TEXTURE_FILTER_NEAREST :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case SRE_TEXTURE_FILTER_LINEAR :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case SRE_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
         }
    if (flags & SRE_TEXTURE_FLAG_ENABLE_WRAP_REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    if (flags & SRE_TEXTURE_FLAG_DISABLE_WRAP_REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
#ifndef OPENGL_ES2
    if (flags & SRE_TEXTURE_FLAG_SET_ANISOTROPY) {
        if (GLEW_EXT_texture_filter_anisotropic) {
            if (anisotropy > max_anisotropy)
                anisotropy = max_anisotropy;
            if (anisotropy < 1.0)
                anisotropy = 1.0;
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        }
    }
#endif
}

void sreTexture::MergeTransparencyMap(sreTexture *t) {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            // Take the r value in the transparency map and assign it to the alpha value in the current texture.
            unsigned int pix = LookupPixel(x, y) + ((0xFF - (t->LookupPixel(x,y) & 0xFF)) << 24);
            SetPixel(x, y, pix);
        }
    UploadGL(TEXTURE_TYPE_NORMAL);
}

unsigned int sreTexture::LookupPixel(int x, int y) {
    if (x < 0)
        x = 0;
    if (x >= width)
        x = width - 1;
    if (y < 0)
        y = 0;
    if (y >= height)
        y = height - 1;
    if (bytes_per_pixel == 4)
        return data[y * width + x];
    int offset = (y * width + x) * 3;
    unsigned char *datap = (unsigned char *)data;
    return (int)datap[offset] | ((int)datap[offset + 1] << 8) | ((int)datap[offset + 2] << 16);
}


void sreTexture::SetPixel(int x, int y, unsigned int value) {
    data[y * width + x] = value;
}

// Create a standard texture for fall-back and test purposes.

static void CreateStandardTexture(int type) {
    sreTexture *tex = new sreTexture(256, 256);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            // Create a reddish checkerboard.
            int tile_on = ((x / 16) + (y / 16)) & 1;
            uint32_t pixel;
            if (tile_on == 0)
                pixel = 0xFF001020;
            else
                pixel = 0xFF2040C0;
            tex->SetPixel(x, y, pixel);
        }
    tex->UploadGL(type);
    if (type == TEXTURE_TYPE_WRAP_REPEAT)
        standard_texture_wrap_repeat = tex;
    else
        standard_texture = tex;
    RegisterTexture(tex, type);
}

sreTexture *sreGetStandardTexture() {
    if (standard_texture == NULL)
        CreateStandardTexture(TEXTURE_TYPE_LINEAR);
    if (standard_texture == NULL) {
        printf("Error -- unable to upload standard texture (OpenGL problem?), giving up.\n");
        exit(1);
    }
    return standard_texture;
}

sreTexture *sreGetStandardTextureWrapRepeat() {
   if (standard_texture_wrap_repeat == NULL)
        CreateStandardTexture(TEXTURE_TYPE_WRAP_REPEAT);
    if (standard_texture_wrap_repeat == NULL) {
        printf("Error -- unable to upload standard texture (OpenGL problem?), giving up.\n");
        exit(1);
    }
    return standard_texture_wrap_repeat;
}

float sreGetMaxAnisotropyLevel() {
   if (max_anisotropy <= 1.0001)
       return 1.0;
   return max_anisotropy;
}

static int nu_registered_textures = 0;
static int capacity = 0;
static sreTexture **registered_textures;

static void RegisterTexture(sreTexture *tex, int type) {
    tex->type = type;
    if (nu_registered_textures == capacity) {
        int new_capacity;
        if (capacity == 0)
            new_capacity = 256;
        else
            new_capacity = capacity * 4;
        sreTexture **new_registered_textures = new sreTexture *[new_capacity];
        if (capacity > 0) {
            memcpy(new_registered_textures, registered_textures,
                sizeof(sreTexture *) * nu_registered_textures);
            delete registered_textures;
        }
        registered_textures = new_registered_textures;
    }
    registered_textures[nu_registered_textures] = tex;
    nu_registered_textures++;
}

void sreScene::ApplyGlobalTextureParameters(int flags, int filter, float anisotropy) {
    printf("Searching list of %d registered textures to apply new texture parameters.\n",
        nu_registered_textures);
    for (int i = 0; i < nu_registered_textures; i++) {
        switch (registered_textures[i]->type) {
        case TEXTURE_TYPE_NORMAL :
        case TEXTURE_TYPE_SRGB :
        case TEXTURE_TYPE_LINEAR :
        case TEXTURE_TYPE_WRAP_REPEAT :
        case TEXTURE_TYPE_USE_RAW_TEXTURE :
            registered_textures[i]->ChangeParameters(flags, filter, anisotropy);
            break;
        }
    }
}
