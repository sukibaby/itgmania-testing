/* RageDisplay_Software: CPU rasterizer implementing the RageDisplay interface.
 *
 * Everything (transform, clip, light, rasterize, texture, blend, depth) runs
 * on the CPU into a system-memory framebuffer. Getting the finished frame onto
 * a window is delegated to a SoftwarePresenter, so this class has no
 * dependency on OpenGL, D3D, or any particular LowLevelWindow.
 */

#ifndef RAGE_DISPLAY_SOFTWARE_H
#define RAGE_DISPLAY_SOFTWARE_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "RageDisplay.h"
#include "RageTypes.h"

/* Backend hook: owns the OS window / blit path. Implement this on top of
 * whatever you like (SDL_UpdateWindowSurface, a GL texture upload, X11
 * XPutImage, Win32 StretchDIBits, ...). */
class SoftwarePresenter {
 public:
  virtual ~SoftwarePresenter() = default;

  /* Create/resize the window for the requested mode. Report the real window
   * size. Return an empty string on success, or an error message. */
  virtual std::string Resize(
      const VideoModeParams& p, int& windowWidthOut, int& windowHeightOut) {
    windowWidthOut = p.width;
    windowHeightOut = p.height;
    return std::string();
  }

  /* Pixels are tightly-rowed 32-bit values, top row first. Byte order in
   * memory is R, G, B, A (i.e. 0xAABBGGRR as a little-endian uint32_t). */
  virtual void Present(
      const uint32_t* pixels, int width, int height, int pitchBytes) = 0;

  virtual void GetDisplaySpecs(DisplaySpecs& /* out */) const {}
  virtual bool SupportsFullscreenBorderlessWindow() const { return false; }
};

/* Blend description (mirrors glBlendFunc / glBlendEquation). */
enum class SwBlendFactor {
  Zero,
  One,
  SrcAlpha,
  OneMinusSrcAlpha,
  SrcColor,
  DstColor,
  OneMinusSrcColor,
  OneMinusDstColor
};
enum class SwBlendEq { Add, Subtract, ReverseSubtract };
struct SwBlend {
  SwBlendFactor src = SwBlendFactor::SrcAlpha;
  SwBlendFactor dst = SwBlendFactor::OneMinusSrcAlpha;
  SwBlendEq eq = SwBlendEq::Add;
  bool separateAlpha = true;  // alpha channel uses (One, OneMinusSrcAlpha)
};

/* A color buffer (+ optional depth). Used for the back buffer, textures and
 * render targets alike. Pixels are packed R | G<<8 | B<<16 | A<<24, row 0 =
 * top of the image (v = 0 when sampled, y = +1 in NDC when rendered to). */
struct SwSurface {
  int w = 0, h = 0;
  std::vector<uint32_t> color;
  std::vector<float> depth;  // empty if no depth buffer
  bool initialized = false;  // has been cleared/rendered at least once

  void Resize(int width, int height, bool withDepth);
};

struct SwTexture : SwSurface {
  bool wrap = true;
  bool filter = true;
};

class RageCompiledGeometrySoftware;

class RageDisplay_Software : public RageDisplay {
  friend class RageCompiledGeometrySoftware;

 public:
  explicit RageDisplay_Software(
      std::unique_ptr<SoftwarePresenter> presenter = nullptr);
  ~RageDisplay_Software() override;

  std::string Init(
      const VideoModeParams& p, bool bAllowUnacceleratedRenderer) override;

  std::string GetApiDescription() const override { return "Software"; }
  void GetDisplaySpecs(DisplaySpecs& out) const override;
  void ResolutionChanged() override;
  const RagePixelFormatDesc* GetPixelFormatDesc(
      RagePixelFormat pf) const override;

  bool BeginFrame() override;
  void EndFrame() override;
  ActualVideoModeParams GetActualVideoModeParams() const override {
    return m_Actual;
  }
  void SetBlendMode(BlendMode mode) override;

  bool SupportsTextureFormat(
      RagePixelFormat pixfmt, bool realtime = false) override;
  bool SupportsPerVertexMatrixScale() override { return false; }

  uintptr_t CreateTexture(
      RagePixelFormat pixfmt, RageSurface* img, bool bGenerateMipMaps) override;
  void UpdateTexture(
      uintptr_t iTexHandle, RageSurface* img, int xoffset, int yoffset,
      int width, int height) override;
  void DeleteTexture(uintptr_t iTexHandle) override;
  void ClearAllTextures() override;
  int GetNumTextureUnits() override { return kNumUnits; }
  void SetTexture(TextureUnit tu, uintptr_t iTexture) override;
  void SetTextureMode(TextureUnit tu, TextureMode tm) override;
  void SetTextureWrapping(TextureUnit tu, bool b) override;
  int GetMaxTextureSize() const override { return 8192; }
  void SetTextureFiltering(TextureUnit tu, bool b) override;

  bool SupportsRenderToTexture() const override { return true; }
  uintptr_t CreateRenderTarget(
      const RenderTargetParam& param, int& iTextureWidthOut,
      int& iTextureHeightOut) override;
  uintptr_t GetRenderTarget() override { return m_hTarget; }
  void SetRenderTarget(uintptr_t iHandle, bool bPreserveTexture) override;

  bool IsZTestEnabled() const override { return m_ZTest != ZTEST_OFF; }
  bool IsZWriteEnabled() const override { return m_bZWrite; }
  void SetZWrite(bool b) override { m_bZWrite = b; }
  void SetZTestMode(ZTestMode mode) override { m_ZTest = mode; }
  void SetZBias(float f) override;
  void ClearZBuffer() override;

  void SetCullMode(CullMode mode) override { m_Cull = mode; }
  void SetAlphaTest(bool b) override { m_bAlphaTest = b; }

  void SetMaterial(
      const RageColor& emissive, const RageColor& ambient,
      const RageColor& diffuse, const RageColor& specular,
      float shininess) override;
  void SetLighting(bool b) override { m_bLighting = b; }
  void SetLightOff(int index) override;
  void SetLightDirectional(
      int index, const RageColor& ambient, const RageColor& diffuse,
      const RageColor& specular, const RageVector3& dir) override;

  void SetSphereEnvironmentMapping(TextureUnit tu, bool b) override;
  void SetCelShaded(int stage) override { m_iCelStage = stage; }

  RageCompiledGeometry* CreateCompiledGeometry() override;
  void DeleteCompiledGeometry(RageCompiledGeometry* p) override;

  void SetPolygonMode(PolygonMode pm) override { m_PolyMode = pm; }
  void SetLineWidth(float fWidth) override { m_fLineWidth = fWidth; }

  RageSurface* CreateScreenshot() override;
  RageSurface* GetTexture(uintptr_t iTexture) override;
  std::string GetTextureDiagnostics(uintptr_t id) const override;

 protected:
  void DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts) override;
  void DrawQuadStripInternal(
      const RageSpriteVertex v[], int iNumVerts) override;
  void DrawFanInternal(const RageSpriteVertex v[], int iNumVerts) override;
  void DrawStripInternal(const RageSpriteVertex v[], int iNumVerts) override;
  void DrawTrianglesInternal(
      const RageSpriteVertex v[], int iNumVerts) override;
  void DrawCompiledGeometryInternal(
      const RageCompiledGeometry* p, int iMeshIndex) override;
  void DrawSymmetricQuadStripInternal(
      const RageSpriteVertex v[], int iNumVerts) override;

  std::string TryVideoMode(
      const VideoModeParams& p, bool& bNewDeviceOut) override;

 private:
  static constexpr int kNumUnits = 2;
  static constexpr int kAttr = 8;  // r g b a  u0 v0  u1 v1
  static constexpr int kMaxLights = 8;

  struct Mat4 {
    float m[16];  // column-major, same layout as RageMatrix / OpenGL
  };
  struct ClipVertex {
    float c[4];
    float a[kAttr];
  };
  struct RasterVertex {
    float x, y, z, invw;
    float a[kAttr];
  };
  struct Light {
    bool enabled = false;
    float ambient[3] = {0, 0, 0};
    float diffuse[3] = {0, 0, 0};
    float specular[3] = {0, 0, 0};
    float toLight[3] = {0, 0, 1};  // eye space, normalized
  };
  struct Material {
    float emissive[4] = {0, 0, 0, 1};
    float ambient[4] = {0.2f, 0.2f, 0.2f, 1};
    float diffuse[4] = {0.8f, 0.8f, 0.8f, 1};
    float specular[4] = {0, 0, 0, 1};
    float shininess = 0;
  };
  struct DrawContext {
    Mat4 modelView, mvp, texture;
    float normalMat[9];  // upper 3x3 of modelView (row-major r*3+c)
  };

  // Per-draw pipeline
  void PrepareDraw();
  void ShadeVertex(
      const float p[3], const float n[3], const float col[4], const float uv[2],
      ClipVertex& out) const;
  void ShadeSpriteVertices(const RageSpriteVertex* v, int n);
  void SubmitTriangle(
      const ClipVertex& a, const ClipVertex& b, const ClipVertex& c);
  void DrawSpriteIndexed(
      const RageSpriteVertex* v, int numVerts, const std::vector<uint32_t>& idx);
  void DrawMesh(const RageCompiledGeometrySoftware* g, int iMeshIndex);
  void DrawScreenLine(
      const RasterVertex& a, const RasterVertex& b, float width);
  void RasterTriangle(
      RasterVertex v0, RasterVertex v1, RasterVertex v2, bool textured);

  SwSurface& Target() { return m_pTarget ? *m_pTarget : m_Back; }

  std::unique_ptr<SoftwarePresenter> m_pPresenter;
  ActualVideoModeParams m_Actual;
  bool m_bDeviceCreated = false;

  SwSurface m_Back;

  uintptr_t m_NextHandle = 1;
  std::unordered_map<uintptr_t, std::unique_ptr<SwTexture>> m_Textures;
  SwTexture* m_pTarget = nullptr;  // null => back buffer
  uintptr_t m_hTarget = 0;
  SwTexture* m_pUnitTex[kNumUnits] = {nullptr, nullptr};
  TextureMode m_UnitMode[kNumUnits];
  bool m_bUnitEnv[kNumUnits] = {false, false};

  SwBlend m_Blend;
  bool m_bZWrite = true;
  ZTestMode m_ZTest = ZTEST_OFF;
  float m_ZNear = 0.0f, m_ZFar = 1.0f;
  CullMode m_Cull = CULL_NONE;
  bool m_bAlphaTest = false;
  bool m_bLighting = false;
  Material m_Material;
  Light m_Lights[kMaxLights];
  float m_SceneAmbient[3] = {0.2f, 0.2f, 0.2f};
  PolygonMode m_PolyMode = POLYGON_FILL;
  float m_fLineWidth = 1.0f;
  int m_iCelStage = -1;  // TODO: cel shading not implemented

  DrawContext m_Ctx;
  std::vector<ClipVertex> m_Shaded;  // scratch
  std::vector<uint32_t> m_Idx;       // scratch
};

#endif
