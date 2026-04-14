/* RageSurface_Load_TGA - TGA file loader */

#ifndef RAGE_SURFACE_LOAD_TGA_H
#define RAGE_SURFACE_LOAD_TGA_H

#include <string>

#include "RageSurface_Load.h"
RageSurfaceUtils::OpenResult RageSurface_Load_TGA(
    const std::string& sPath, RageSurface*& ret, bool bHeaderOnly,
    std::string& error);

#endif