#include "RageSurface_Load_TGA.h"

#include "RageSurface_Load_STB.h"

RageSurfaceUtils::OpenResult RageSurface_Load_TGA(
    const std::string& sPath, RageSurface*& ret, bool bHeaderOnly,
    std::string& error) {
  return RageSurface_Load_STB(
      sPath, ret, bHeaderOnly, error, RageSurfaceSTBFormat::TGA);
}