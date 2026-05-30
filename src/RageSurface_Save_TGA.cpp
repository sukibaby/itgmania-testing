#include "RageSurface_Save_TGA.h"

#include <string>

#include "RageFile.h"
#include "RageSurface_Save_STB.h"

bool RageSurfaceUtils::SaveTGA(
    RageSurface* surface, RageFile& f, std::string& sError) {
  return RageSurface_Save_STB(
      surface, f, sError, RageSurfaceSTBWriteFormat::TGA);
}

bool RageSurfaceUtils::SaveTGA(RageSurface* surface, RageFile& f) {
  std::string error;
  return SaveTGA(surface, f, error);
}