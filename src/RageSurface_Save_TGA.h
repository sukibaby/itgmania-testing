/* RageSurface_Save_TGA - Save a RageSurface to a TGA. */

#ifndef RAGE_SURFACE_SAVE_TGA_H
#define RAGE_SURFACE_SAVE_TGA_H

#include <string>

struct RageSurface;
class RageFile;
namespace RageSurfaceUtils {
bool SaveTGA(RageSurface* surface, RageFile& f, std::string& sError);
bool SaveTGA(RageSurface* surface, RageFile& f);
};  // namespace RageSurfaceUtils

#endif