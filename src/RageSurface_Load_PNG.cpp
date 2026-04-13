#include "RageSurface_Load_PNG.h"

#include <png.h>

#include <climits>
#include <csetjmp>
#include <cstdint>
#include <cstring>
#include <string>

#include "RageFile.h"
#include "RageLog.h"
#include "RageSurface.h"
#include "RageSurface_Load.h"
#include "RageUtil.h"
#include "RageUtil/Endian.h"
#include "global.h"
#include "pngconf.h"

#if defined(_MSC_VER)
#if defined(_BINARY_PNG)
#pragma comment(lib, "libpng.lib")
#endif
#pragma warning(disable : 4611) /* interaction between '_setjmp' and C++ \
                                   object destruction is non-portable */
#endif                          // _MSC_VER

namespace {
constexpr png_size_t kPngReadBufferSize = 64 * 1024;

void CopyStringZ(char* dst, size_t dst_size, const char* src) {
  if (dst_size == 0) {
    return;
  }

  if (src == nullptr) {
    dst[0] = 0;
    return;
  }

  size_t len = std::strlen(src);
  if (len >= dst_size) {
    len = dst_size - 1;
  }

  if (len != 0) {
    std::memcpy(dst, src, len);
  }
  dst[len] = 0;
}

struct error_info {
  char* err;
  size_t err_size;
  const char* fn;
};

struct png_read_state {
  RageFile* file;
  png_size_t buffer_offset;
  png_size_t buffer_size;
  png_byte buffer[kPngReadBufferSize];
};

void PNG_CopyFileError(png_struct* png, RageFile* file) {
  thread_local char error[256];
  CopyStringZ(error, sizeof(error), file->GetError().c_str());
  png_error(png, error);
}

void PNG_ReadFile(
    png_struct* png, RageFile* file, png_byte* dst, png_size_t size) {
  while (size != 0) {
    size_t request_size = static_cast<size_t>(size);
    if (request_size > static_cast<size_t>(INT_MAX)) {
      request_size = INT_MAX;
    }

    const int got = file->Read(dst, request_size);
    if (got == -1) {
      PNG_CopyFileError(png, file);
    }
    if (got <= 0) {
      png_error(png, "Unexpected EOF");
    }

    dst += got;
    size -= got;
  }
}

void RageFile_png_read(png_struct* png, png_byte* p, png_size_t size) {
  png_read_state* state = (png_read_state*)png_get_io_ptr(png);

  while (size != 0) {
    png_size_t available = state->buffer_size - state->buffer_offset;
    if (available == 0) {
      if (size >= kPngReadBufferSize) {
        PNG_ReadFile(png, state->file, p, size);
        return;
      }

      const int got = state->file->Read(state->buffer, sizeof(state->buffer));
      if (got == -1) {
        PNG_CopyFileError(png, state->file);
      }
      if (got <= 0) {
        png_error(png, "Unexpected EOF");
      }

      state->buffer_offset = 0;
      state->buffer_size = static_cast<png_size_t>(got);
      available = state->buffer_size;
    }

    png_size_t copy_size = available;
    if (copy_size > size) {
      copy_size = size;
    }

    std::memcpy(p, state->buffer + state->buffer_offset, copy_size);
    state->buffer_offset += copy_size;
    p += copy_size;
    size -= copy_size;
  }
}

void PNG_Error(png_struct* png, const char* error) {
  error_info* info = (error_info*)png_get_error_ptr(png);
  CopyStringZ(info->err, info->err_size, error);
  LOG->Trace("loading \"%s\": err: %s", info->fn, info->err);
  longjmp(png_jmpbuf(png), 1);
}

void PNG_Warning(png_struct* png, const char* warning) {
  error_info* info = (error_info*)png_get_error_ptr(png);
  LOG->Trace("loading \"%s\": warning: %s", info->fn, warning);
}

/* Since libpng forces us to use longjmp (gross!), this function shouldn't
 * create any C++ objects, and needs to watch out for memleaks. */
static RageSurface* RageSurface_Load_PNG(
    RageFile* f, const char* fn, char errorbuf[1024], bool bHeaderOnly) {
  error_info error;
  error.err = errorbuf;
  error.err_size = 1024;
  error.fn = fn;

  png_read_state read_state;
  read_state.file = f;
  read_state.buffer_offset = 0;
  read_state.buffer_size = 0;

  png_struct* png = png_create_read_struct(
      PNG_LIBPNG_VER_STRING, &error, PNG_Error, PNG_Warning);

  if (png == nullptr) {
    CopyStringZ(
        errorbuf, error.err_size, "creating png_create_read_struct failed");
    return nullptr;
  }

  png_info* info_ptr = png_create_info_struct(png);
  if (info_ptr == nullptr) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    CopyStringZ(
        errorbuf, error.err_size, "creating png_create_info_struct failed");
    return nullptr;
  }

  RageSurface* volatile img = nullptr;

  // Throwing an exception in the error callback would make the exception
  // pass through C code, which is undefined behavior.  Works fine on Linux,
  // and on macOS with C++11, but does not work on macOS without C++11. -Kyz
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info_ptr, nullptr);
    delete img;
    return nullptr;
  }

  png_set_read_fn(png, &read_state, RageFile_png_read);

  png_read_info(png, info_ptr);

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  png_get_IHDR(
      png, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type,
      nullptr, nullptr);

  /* If bHeaderOnly is true, don't allocate the pixel storage space or
   * decompress the image.  Just return an empty surface with only the width and
   * height set. */
  if (bHeaderOnly) {
    img = CreateSurfaceFrom(width, height, 32, 0, 0, 0, 0, nullptr, width * 4);
    png_destroy_read_struct(&png, &info_ptr, nullptr);
    return img;
  }

  png_set_strip_16(png); /* 16bit->8bit */
  png_set_packing(png);  /* 1,2,4 bit->8 bit */

  /* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }

  /* These are set for type == PALETTE. */
  RageSurfaceColor colors[256] = {};
  int iColorKey = -1;

  /* We import three types of files: paletted, RGBX and RGBA.  The only
   * difference between RGBX and RGBA is that RGBX won't set the alpha mask, so
   * it's easier to tell later on that there's no alpha (without actually having
   * to do a pixel scan). */
  enum { PALETTE, RGBX, RGBA } type;
  switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
      /* Fake PNG_COLOR_TYPE_GRAY. */
      for (int i = 0; i < 256; ++i) {
        colors[i].r = colors[i].g = colors[i].b = (int8_t)i;
        colors[i].a = 0xFF;
      }

      type = PALETTE;
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
      type = RGBA;
      png_set_gray_to_rgb(png);
      break;
    case PNG_COLOR_TYPE_PALETTE:
      type = PALETTE;
      break;
    case PNG_COLOR_TYPE_RGB:
      type = RGBX;
      break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
      type = RGBA;
      break;
    default:
      FAIL_M(ssprintf("%i", color_type));
  }

  if (color_type == PNG_COLOR_TYPE_GRAY) {
    png_color_16* trans;
    if (png_get_tRNS(png, info_ptr, nullptr, nullptr, &trans) ==
        PNG_INFO_tRNS) {
      iColorKey = trans->gray;
      if (bit_depth == 16) {
        iColorKey >>= 8;
      } else if (bit_depth < 8) {
        const int max_value = (1 << bit_depth) - 1;
        iColorKey = (iColorKey * 255 + max_value / 2) / max_value;
      }
    }
  } else if (color_type == PNG_COLOR_TYPE_PALETTE) {
    int num_palette;
    png_color* palette;
    int ret = png_get_PLTE(png, info_ptr, &palette, &num_palette);
    ASSERT(ret == PNG_INFO_PLTE);

    png_byte* trans = nullptr;
    int num_trans = 0;
    png_get_tRNS(png, info_ptr, &trans, &num_trans, nullptr);

    for (int i = 0; i < num_palette; ++i) {
      colors[i].r = palette[i].red;
      colors[i].g = palette[i].green;
      colors[i].b = palette[i].blue;
      colors[i].a = 0xFF;
      if (i < num_trans) {
        colors[i].a = trans[i];
      }
    }
  } else {
    /* If we have RGB image and tRNS, it's a color key.  Just convert it to
     * RGBA. */
    if (png_get_valid(png, info_ptr, PNG_INFO_tRNS)) {
      /* We don't care about RGB color keys; just convert them to alpha. */
      png_set_tRNS_to_alpha(png);
      type = RGBA;
    }

    /* RGB->RGBX */
    png_set_filler(png, 0xff, PNG_FILLER_AFTER);
  }

  int passes = 1;
  if (interlace_type != PNG_INTERLACE_NONE) {
    passes = png_set_interlace_handling(png);
  }

  png_read_update_info(png, info_ptr);

  switch (type) {
    case PALETTE:
      img = CreateSurface(width, height, 8, 0, 0, 0, 0);
      memcpy(img->fmt.palette->colors, colors, 256 * sizeof(RageSurfaceColor));

      if (iColorKey != -1) {
        img->format->palette->colors[iColorKey].a = 0;
      }

      break;
    case RGBX:
    case RGBA:
      img = CreateSurface(
          width, height, 32, Swap32BE(0xFF000000), Swap32BE(0x00FF0000),
          Swap32BE(0x0000FF00),
          Swap32BE(type == RGBA ? 0x000000FF : 0x00000000));
      break;
    default:
      FAIL_M(ssprintf("%i", type));
  }
  ASSERT(img != nullptr);
  ASSERT(
      static_cast<png_uint_32>(img->pitch) >= png_get_rowbytes(png, info_ptr));

  png_byte* pixels = (png_byte*)img->pixels;
  for (int pass = 0; pass < passes; ++pass) {
    for (png_uint_32 y = 0; y < height; ++y) {
      png_byte* row = pixels + img->pitch * y;
      png_read_row(png, row, nullptr);
    }
  }

  png_destroy_read_struct(&png, &info_ptr, nullptr);

  return img;
}

};  // namespace

RageSurfaceUtils::OpenResult RageSurface_Load_PNG(
    const std::string& sPath, RageSurface*& ret, bool bHeaderOnly,
    std::string& error) {
  RageFile f;
  if (!f.Open(sPath)) {
    error = f.GetError();
    return RageSurfaceUtils::OPEN_FATAL_ERROR;
  }

  char errorbuf[1024];
  ret = RageSurface_Load_PNG(&f, sPath.c_str(), errorbuf, bHeaderOnly);
  if (ret == nullptr) {
    error = errorbuf;
    return RageSurfaceUtils::OPEN_UNKNOWN_FILE_FORMAT;  // XXX
  }

  return RageSurfaceUtils::OPEN_OK;
}

/*
 * (c) 2004 Glenn Maynard
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
