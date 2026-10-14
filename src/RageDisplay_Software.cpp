#include "RageDisplay_Software.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <vector>

#include "RageMath.h"
#include "RageSurface.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "archutils/Win32/GraphicsWindow.h"
#endif

/* ------------------------------------------------------------------------ */
/* Small helpers                                                            */
/* ------------------------------------------------------------------------ */
namespace {

static_assert(
    sizeof(RageMatrix) == sizeof(float) * 16,
  "RageMatrix must be represented by 16 packed floats");

inline float Clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

inline uint32_t Pack(const float c[4]) {
  auto q = [](float x) { return (uint32_t)(Clamp01(x) * 255.0f + 0.5f); };
  return q(c[0]) | (q(c[1]) << 8) | (q(c[2]) << 16) | (q(c[3]) << 24);
}
inline void Unpack(uint32_t p, float out[4]) {
  const float k = 1.0f / 255.0f;
  out[0] = (float)(p & 0xFF) * k;
  out[1] = (float)((p >> 8) & 0xFF) * k;
  out[2] = (float)((p >> 16) & 0xFF) * k;
  out[3] = (float)((p >> 24) & 0xFF) * k;
}

/* RageSpriteVertex::c is a byte color in some engine versions and a float
 * color in others; handle both. */
template <class C>
inline void ColorToFloat4(const C& c, float out[4]) {
  using T = std::decay_t<decltype(c.r)>;
  if constexpr (std::is_integral_v<T>) {
    const float k = 1.0f / 255.0f;
    out[0] = c.r * k;
    out[1] = c.g * k;
    out[2] = c.b * k;
    out[3] = c.a * k;
  } else {
    out[0] = c.r;
    out[1] = c.g;
    out[2] = c.b;
    out[3] = c.a;
  }
}

inline void ColorToFloat4(const RageColor& c, float out[4]) {
  out[0] = c.r;
  out[1] = c.g;
  out[2] = c.b;
  out[3] = c.a;
}

inline int WrapIndex(int i, int n, bool wrap) {
  if (wrap) {
    i %= n;
    return i < 0 ? i + n : i;
  }
  return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

inline int FloorToInt(float f) {
  if (!(f > -1e9f)) f = -1e9f;  // also catches NaN
  if (f > 1e9f) f = 1e9f;
  return (int)std::floor(f);
}

void SampleTexture(const SwTexture& t, float u, float v, float out[4]) {
  const int W = t.w, H = t.h;
  if (!t.filter) {
    int x = WrapIndex(FloorToInt(u * (float)W), W, t.wrap);
    int y = WrapIndex(FloorToInt(v * (float)H), H, t.wrap);
    Unpack(t.color[(size_t)y * W + x], out);
    return;
  }
  float fu = u * (float)W - 0.5f, fv = v * (float)H - 0.5f;
  int x0 = FloorToInt(fu), y0 = FloorToInt(fv);
  float tx = fu - (float)x0, ty = fv - (float)y0;
  int xa = WrapIndex(x0, W, t.wrap), xb = WrapIndex(x0 + 1, W, t.wrap);
  int ya = WrapIndex(y0, H, t.wrap), yb = WrapIndex(y0 + 1, H, t.wrap);
  float c00[4], c10[4], c01[4], c11[4];
  Unpack(t.color[(size_t)ya * W + xa], c00);
  Unpack(t.color[(size_t)ya * W + xb], c10);
  Unpack(t.color[(size_t)yb * W + xa], c01);
  Unpack(t.color[(size_t)yb * W + xb], c11);
  for (int i = 0; i < 4; ++i) {
    float top = c00[i] + (c10[i] - c00[i]) * tx;
    float bot = c01[i] + (c11[i] - c01[i]) * tx;
    out[i] = top + (bot - top) * ty;
  }
}

float BlendFactor(
    SwBlendFactor f, const float s[4], const float d[4], int ch) {
  switch (f) {
    case SwBlendFactor::Zero: return 0.f;
    case SwBlendFactor::One: return 1.f;
    case SwBlendFactor::SrcAlpha: return s[3];
    case SwBlendFactor::OneMinusSrcAlpha: return 1.f - s[3];
    case SwBlendFactor::SrcColor: return s[ch];
    case SwBlendFactor::DstColor: return d[ch];
    case SwBlendFactor::OneMinusSrcColor: return 1.f - s[ch];
    case SwBlendFactor::OneMinusDstColor: return 1.f - d[ch];
  }
  return 1.f;
}

inline float Combine(SwBlendEq eq, float s, float fs, float d, float fd) {
  switch (eq) {
    case SwBlendEq::Add: return s * fs + d * fd;
    case SwBlendEq::Subtract: return s * fs - d * fd;
    case SwBlendEq::ReverseSubtract: return d * fd - s * fs;
  }
  return 0.f;
}

/* Read one pixel of an arbitrary RageSurface as 8-bit RGBA. */
struct ChannelInfo {
  uint32_t mask = 0;
  int shift = 0;
  int bits = 0;
  void Init(uint32_t m) {
    mask = m;
    shift = 0;
    bits = 0;
    if (!m) return;
    while (!((m >> shift) & 1u)) ++shift;
    uint32_t v = m >> shift;
    while (v & 1u) {
      ++bits;
      v >>= 1;
    }
  }
  uint8_t Get(uint32_t pix, uint8_t missing) const {
    if (!mask) return missing;
    uint32_t v = (pix & mask) >> shift;
    uint32_t maxv = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    return (uint8_t)((v * 255u + maxv / 2u) / maxv);
  }
};

}  // namespace

void SwSurface::Resize(int width, int height, bool withDepth) {
  w = std::max(width, 1);
  h = std::max(height, 1);
  color.assign((size_t)w * h, 0u);
  if (withDepth) {
    depth.assign((size_t)w * h, 1.0f);
  } else {
    depth.clear();
  }
  initialized = false;
}

namespace {

#if defined(_WIN32)
class SoftwarePresenterNative final : public SoftwarePresenter {
 public:
  SoftwarePresenterNative() { GraphicsWindow::Initialize(false); }

  ~SoftwarePresenterNative() override { GraphicsWindow::Shutdown(); }

  std::string Resize(
      const VideoModeParams& params, int& windowWidthOut,
      int& windowHeightOut) override {
    if (GraphicsWindow::GetHwnd() == nullptr) {
      GraphicsWindow::CreateGraphicsWindow(params);
    }

    const std::string error = GraphicsWindow::SetScreenMode(params);
    if (!error.empty()) {
      return error;
    }

    GraphicsWindow::CreateGraphicsWindow(params);
    const VideoModeParams& actual = GraphicsWindow::GetParams();
    windowWidthOut = actual.width;
    windowHeightOut = actual.height;
    return std::string();
  }

  void Present(
      const uint32_t* pixels, int width, int height, int pitchBytes) override {
    if (GraphicsWindow::GetHwnd() == nullptr || pixels == nullptr ||
        width <= 0 || height <= 0 ||
        pitchBytes < width * static_cast<int>(sizeof(uint32_t))) {
      return;
    }

    m_dibPixels.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
      const auto* source = reinterpret_cast<const uint32_t*>(
          reinterpret_cast<const uint8_t*>(pixels) +
          static_cast<size_t>(y) * pitchBytes);
      uint32_t* destination =
          m_dibPixels.data() + static_cast<size_t>(y) * width;
      for (int x = 0; x < width; ++x) {
        const uint32_t pixel = source[x];
        destination[x] = 0xFF000000u | (pixel & 0x0000FF00u) |
                          ((pixel & 0x000000FFu) << 16) |
                          ((pixel & 0x00FF0000u) >> 16);
      }
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(GraphicsWindow::GetHDC(), COLORONCOLOR);
    StretchDIBits(
        GraphicsWindow::GetHDC(), 0, 0, width, height, 0, 0, width, height,
        m_dibPixels.data(), &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
  }
  void Update() override { GraphicsWindow::Update(); }

  bool GetActualSize(int& widthOut, int& heightOut) const override {
    if (GraphicsWindow::GetHwnd() == nullptr) {
      return false;
    }
    const VideoModeParams& actual = GraphicsWindow::GetParams();
    widthOut = actual.width;
    heightOut = actual.height;
    return widthOut > 0 && heightOut > 0;
  }

  void GetDisplaySpecs(DisplaySpecs& out) const override {
    GraphicsWindow::GetDisplaySpecs(out);
  }

  bool SupportsFullscreenBorderlessWindow() const override {
    return true;
  }

 private:
  std::vector<uint32_t> m_dibPixels;
};
#else
class SoftwarePresenterNative final : public SoftwarePresenter {
 public:
  std::string Resize(
      const VideoModeParams& /* params */, int& /* windowWidthOut */,
      int& /* windowHeightOut */) override {
    return "Native software presentation is not implemented on this platform";
  }

  void Present(
      const uint32_t* /* pixels */, int /* width */, int /* height */,
      int /* pitchBytes */) override {}
};
#endif

}  // namespace

/* ------------------------------------------------------------------------ */
/* Compiled geometry                                                        */
/* ------------------------------------------------------------------------ */
class RageCompiledGeometrySoftware : public RageCompiledGeometry {
 public:
  explicit RageCompiledGeometrySoftware(RageDisplay_Software* owner)
      : m_pOwner(owner) {}

  void Allocate(const std::vector<msMesh>& /* vMeshes */) override {
    m_Vertices.resize(GetTotalVertices());
    m_Indices.resize(GetTotalTriangles() * 3);
  }

  void Change(const std::vector<msMesh>& vMeshes) override {
    for (size_t i = 0; i < vMeshes.size(); ++i) {
      const msMesh& mesh = vMeshes[i];
      const MeshInfo& info = m_vMeshInfo[i];
      for (size_t j = 0; j < mesh.Vertices.size(); ++j) {
        m_Vertices[info.iVertexStart + j] = mesh.Vertices[j];
      }
      for (size_t j = 0; j < mesh.Triangles.size(); ++j) {
        for (int k = 0; k < 3; ++k) {
          m_Indices[(info.iTriangleStart + j) * 3 + k] =
              (uint32_t)(mesh.Triangles[j].nVertexIndices[k] +
                         info.iVertexStart);
        }
      }
    }
  }

  void Draw(int iMeshIndex) const override {
    m_pOwner->DrawMesh(this, iMeshIndex);
  }

  const MeshInfo& Info(int i) const { return m_vMeshInfo[i]; }
  const RageModelVertex& Vertex(size_t i) const { return m_Vertices[i]; }
  const uint32_t* Indices() const { return m_Indices.data(); }

 private:
  RageDisplay_Software* m_pOwner;
  std::vector<RageModelVertex> m_Vertices;
  std::vector<uint32_t> m_Indices;
};

/* ------------------------------------------------------------------------ */
/* Construction / video mode                                                */
/* ------------------------------------------------------------------------ */
RageDisplay_Software::RageDisplay_Software(
    std::unique_ptr<SoftwarePresenter> presenter)
    : m_pPresenter(std::move(presenter)) {
  if (!m_pPresenter) {
      m_pPresenter = std::make_unique<SoftwarePresenterNative>();
  }
  for (int i = 0; i < kNumUnits; ++i) m_UnitMode[i] = TextureMode_Modulate;
  std::memset(&m_Ctx, 0, sizeof(m_Ctx));
}

RageDisplay_Software::~RageDisplay_Software() {}

std::string RageDisplay_Software::Init(
    const VideoModeParams& p, bool /* bAllowUnacceleratedRenderer */) {
  bool bNeedReload = false;
  return SetVideoMode(p, bNeedReload);
}

std::string RageDisplay_Software::TryVideoMode(
    const VideoModeParams& p, bool& bNewDeviceOut) {
  int ww = p.width, wh = p.height;
  if (m_pPresenter) {
    std::string err = m_pPresenter->Resize(p, ww, wh);
    if (!err.empty()) return err;
  }
  m_Actual = ActualVideoModeParams(p, ww, wh, false);
  m_Back.Resize(ww, wh, true);
  bNewDeviceOut = !m_bDeviceCreated;  // textures live in system RAM and survive
  m_bDeviceCreated = true;
  return std::string();
}

void RageDisplay_Software::GetDisplaySpecs(DisplaySpecs& out) const {
  if (m_pPresenter) m_pPresenter->GetDisplaySpecs(out);
}

void RageDisplay_Software::ResolutionChanged() {
  int width = 0;
  int height = 0;
  if (m_pPresenter && m_pPresenter->GetActualSize(width, height) &&
      (width != m_Back.w || height != m_Back.h)) {
    m_Back.Resize(width, height, true);
    m_Actual.width = width;
    m_Actual.height = height;
  }
  RageDisplay::ResolutionChanged();
}

const RageDisplay::RagePixelFormatDesc* RageDisplay_Software::GetPixelFormatDesc(
    RagePixelFormat pf) const {
  /* Masks are interpreted against native-endian words of `bpp` bits, which is
   * how RageSurface describes pixels. Texture upload converts through the
   * surface's own masks, so any consistent choice works. */
  static const RagePixelFormatDesc d[NUM_RagePixelFormat] = {
      /* RGBA8  */ {32, {0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu}},
      /* BGRA8  */ {32, {0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u}},
      /* RGBA4  */ {16, {0xF000u, 0x0F00u, 0x00F0u, 0x000Fu}},
      /* RGB5A1 */ {16, {0xF800u, 0x07C0u, 0x003Eu, 0x0001u}},
      /* RGB5   */ {16, {0xF800u, 0x07E0u, 0x001Fu, 0x0000u}},
      /* RGB8   */ {24, {0xFF0000u, 0x00FF00u, 0x0000FFu, 0x000000u}},
      /* PAL    */ {8, {0, 0, 0, 0}},
      /* BGR8   */ {24, {0x0000FFu, 0x00FF00u, 0xFF0000u, 0x000000u}},
      /* A1BGR5 */ {16, {0x001Fu, 0x07C0u, 0xF800u, 0x0001u}},
      /* X1RGB5 */ {16, {0x7C00u, 0x03E0u, 0x001Fu, 0x0000u}},
  };
  return &d[pf];
}

bool RageDisplay_Software::SupportsTextureFormat(
    RagePixelFormat pixfmt, bool /* realtime */) {
  switch (pixfmt) {
    case RagePixelFormat_RGBA8:
    case RagePixelFormat_BGRA8:
    case RagePixelFormat_RGBA4:
    case RagePixelFormat_RGB5A1:
    case RagePixelFormat_RGB5:
    case RagePixelFormat_RGB8:
    case RagePixelFormat_PAL:
      return true;
    default:
      return false;
  }
}

/* ------------------------------------------------------------------------ */
/* Frame                                                                    */
/* ------------------------------------------------------------------------ */
bool RageDisplay_Software::BeginFrame() {
  m_pTarget = nullptr;
  m_hTarget = 0;
  std::fill(m_Back.color.begin(), m_Back.color.end(), 0xFF000000u);
  std::fill(m_Back.depth.begin(), m_Back.depth.end(), 1.0f);
  m_Back.initialized = true;
  return RageDisplay::BeginFrame();
}

void RageDisplay_Software::EndFrame() {
  FrameLimitBeforeVsync();
  if (m_pPresenter) {
    m_pPresenter->Present(
        m_Back.color.data(), m_Back.w, m_Back.h, m_Back.w * (int)sizeof(uint32_t));
    m_pPresenter->Update();
  }
  FrameLimitAfterVsync();
  RageDisplay::EndFrame();
}

/* ------------------------------------------------------------------------ */
/* Blend / depth / material / lights                                        */
/* ------------------------------------------------------------------------ */
void RageDisplay_Software::SetBlendMode(BlendMode mode) {
  using F = SwBlendFactor;
  SwBlend b;
  b.eq = SwBlendEq::Add;
  b.separateAlpha = false;
  switch (mode) {
    case BLEND_NORMAL:
      b.src = F::SrcAlpha;
      b.dst = F::OneMinusSrcAlpha;
      b.separateAlpha = true;
      break;
    case BLEND_ADD:
      b.src = F::SrcAlpha;
      b.dst = F::One;
      break;
    case BLEND_SUBTRACT:
      b.src = F::SrcAlpha;
      b.dst = F::One;
      b.eq = SwBlendEq::ReverseSubtract;
      break;
    case BLEND_MODULATE:
      b.src = F::Zero;
      b.dst = F::SrcColor;
      break;
    case BLEND_COPY_SRC:
      b.src = F::One;
      b.dst = F::Zero;
      break;
    case BLEND_WEIGHTED_MULTIPLY:
      b.src = F::DstColor;
      b.dst = F::SrcColor;
      break;
    case BLEND_INVERT_DEST:  // UNVERIFIED against RageDisplay_OGL.cpp
      b.src = F::One;
      b.dst = F::One;
      b.eq = SwBlendEq::Subtract;
      break;
    case BLEND_NO_EFFECT:
      b.src = F::Zero;
      b.dst = F::One;
      break;
    case BLEND_ALPHA_MASK:  // UNVERIFIED
      b.src = F::Zero;
      b.dst = F::SrcAlpha;
      break;
    case BLEND_ALPHA_KNOCK_OUT:  // UNVERIFIED
      b.src = F::Zero;
      b.dst = F::OneMinusSrcAlpha;
      break;
    case BLEND_ALPHA_MULTIPLY:  // UNVERIFIED
      b.src = F::DstColor;
      b.dst = F::Zero;
      break;
    default:
      break;
  }
  m_Blend = b;
}

void RageDisplay_Software::SetZBias(float f) {
  /* Same range squeeze the OpenGL backend applies via glDepthRange. */
  m_ZNear = f * 0.05f;
  m_ZFar = 1.0f - f * 0.05f;
}

void RageDisplay_Software::ClearZBuffer() {
  SwSurface& t = Target();
  std::fill(t.depth.begin(), t.depth.end(), 1.0f);
}

void RageDisplay_Software::SetMaterial(
    const RageColor& emissive, const RageColor& ambient,
    const RageColor& diffuse, const RageColor& specular, float shininess) {
  ColorToFloat4(emissive, m_Material.emissive);
  ColorToFloat4(ambient, m_Material.ambient);
  ColorToFloat4(diffuse, m_Material.diffuse);
  ColorToFloat4(specular, m_Material.specular);
  m_Material.shininess = shininess;
}

void RageDisplay_Software::SetLightOff(int index) {
  if (index >= 0 && index < kMaxLights) m_Lights[index].enabled = false;
}

void RageDisplay_Software::SetLightDirectional(
    int index, const RageColor& ambient, const RageColor& diffuse,
    const RageColor& specular, const RageVector3& dir) {
  if (index < 0 || index >= kMaxLights) return;
  Light& l = m_Lights[index];
  float c[4];
  l.enabled = true;
  ColorToFloat4(ambient, c);
  std::memcpy(l.ambient, c, sizeof(l.ambient));
  ColorToFloat4(diffuse, c);
  std::memcpy(l.diffuse, c, sizeof(l.diffuse));
  ColorToFloat4(specular, c);
  std::memcpy(l.specular, c, sizeof(l.specular));
  /* Direction the light travels -> vector from surface toward light. Treated
   * as eye space (OpenGL backend sets it under an identity modelview). */
  float x = -dir.x, y = -dir.y, z = -dir.z;
  float len = std::sqrt(x * x + y * y + z * z);
  if (len > 1e-8f) {
    x /= len;
    y /= len;
    z /= len;
  }
  l.toLight[0] = x;
  l.toLight[1] = y;
  l.toLight[2] = z;
}

void RageDisplay_Software::SetSphereEnvironmentMapping(TextureUnit tu, bool b) {
  if ((int)tu < kNumUnits) m_bUnitEnv[tu] = b;
}

/* ------------------------------------------------------------------------ */
/* Textures                                                                 */
/* ------------------------------------------------------------------------ */
namespace {

/* Convert (a sub-rect of) a RageSurface into packed RGBA8. */
void ConvertSurfaceRegion(
    const RageSurface* img, int width, int height, uint32_t* dst,
    int dstPitchPixels) {
  const auto& f = img->fmt;
  const int bpp = f.BytesPerPixel;
  ChannelInfo ch[4];
  for (int i = 0; i < 4; ++i) ch[i].Init(f.Mask[i]);
  const bool paletted = (bpp == 1 && f.palette != nullptr);

  for (int y = 0; y < height; ++y) {
    const uint8_t* row = img->pixels + (size_t)y * img->pitch;
    uint32_t* out = dst + (size_t)y * dstPitchPixels;
    for (int x = 0; x < width; ++x) {
      const uint8_t* px = row + (size_t)x * bpp;
      if (paletted) {
        const auto& c = f.palette->colors[px[0]];
        out[x] = (uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) |
                 ((uint32_t)c.a << 24);
        continue;
      }
      uint32_t pix = 0;
      for (int b = 0; b < bpp && b < 4; ++b) pix |= (uint32_t)px[b] << (8 * b);
      out[x] = (uint32_t)ch[0].Get(pix, 0) | ((uint32_t)ch[1].Get(pix, 0) << 8) |
               ((uint32_t)ch[2].Get(pix, 0) << 16) |
               ((uint32_t)ch[3].Get(pix, 255) << 24);
    }
  }
}

}  // namespace

uintptr_t RageDisplay_Software::CreateTexture(
    RagePixelFormat /* pixfmt */, RageSurface* img, bool /* bGenerateMipMaps */) {
  if (!img || img->w <= 0 || img->h <= 0) return 0;
  auto tex = std::make_unique<SwTexture>();
  tex->Resize(img->w, img->h, false);
  ConvertSurfaceRegion(img, img->w, img->h, tex->color.data(), tex->w);
  tex->initialized = true;
  uintptr_t h = m_NextHandle++;
  m_Textures[h] = std::move(tex);
  return h;  // TODO: mipmaps (bGenerateMipMaps is ignored)
}

void RageDisplay_Software::UpdateTexture(
    uintptr_t iTexHandle, RageSurface* img, int xoffset, int yoffset, int width,
    int height) {
  auto it = m_Textures.find(iTexHandle);
  if (it == m_Textures.end() || !img) return;
  SwTexture& t = *it->second;
  /* Same clipping glTexSubImage2D would require; source (0,0) lands on
   * (xoffset, yoffset). */
  if (xoffset < 0 || yoffset < 0) return;
  width = std::min(width, std::min(t.w - xoffset, img->w));
  height = std::min(height, std::min(t.h - yoffset, img->h));
  if (width <= 0 || height <= 0) return;
  ConvertSurfaceRegion(
      img, width, height, t.color.data() + (size_t)yoffset * t.w + xoffset, t.w);
}

void RageDisplay_Software::DeleteTexture(uintptr_t iTexHandle) {
  auto it = m_Textures.find(iTexHandle);
  if (it == m_Textures.end()) return;
  SwTexture* p = it->second.get();
  for (auto& u : m_pUnitTex) {
    if (u == p) u = nullptr;
  }
  if (m_pTarget == p) {
    m_pTarget = nullptr;
    m_hTarget = 0;
  }
  m_Textures.erase(it);
}

void RageDisplay_Software::ClearAllTextures() {
  for (auto& u : m_pUnitTex) u = nullptr;
}

void RageDisplay_Software::SetTexture(TextureUnit tu, uintptr_t iTexture) {
  if ((int)tu >= kNumUnits) return;
  auto it = m_Textures.find(iTexture);
  m_pUnitTex[tu] = (iTexture && it != m_Textures.end()) ? it->second.get() : nullptr;
}

void RageDisplay_Software::SetTextureMode(TextureUnit tu, TextureMode tm) {
  if ((int)tu < kNumUnits) m_UnitMode[tu] = tm;
}

/* As in OpenGL, wrap/filter are properties of the bound texture object. */
void RageDisplay_Software::SetTextureWrapping(TextureUnit tu, bool b) {
  if ((int)tu < kNumUnits && m_pUnitTex[tu]) m_pUnitTex[tu]->wrap = b;
}

void RageDisplay_Software::SetTextureFiltering(TextureUnit tu, bool b) {
  if ((int)tu < kNumUnits && m_pUnitTex[tu]) m_pUnitTex[tu]->filter = b;
}

RageSurface* RageDisplay_Software::GetTexture(uintptr_t iTexture) {
  auto it = m_Textures.find(iTexture);
  if (it == m_Textures.end()) return nullptr;
  const SwTexture& t = *it->second;
  const RagePixelFormatDesc* d = GetPixelFormatDesc(RagePixelFormat_RGBA8);
  RageSurface* s =
      CreateSurface(t.w, t.h, d->bpp, d->masks[0], d->masks[1], d->masks[2], d->masks[3]);
  for (int y = 0; y < t.h; ++y) {
    uint32_t* dst = (uint32_t*)(s->pixels + (size_t)y * s->pitch);
    for (int x = 0; x < t.w; ++x) {
      uint32_t p = t.color[(size_t)y * t.w + x];
      dst[x] = ((p & 0xFF) << 24) | (((p >> 8) & 0xFF) << 16) |
               (((p >> 16) & 0xFF) << 8) | ((p >> 24) & 0xFF);
    }
  }
  return s;
}

std::string RageDisplay_Software::GetTextureDiagnostics(uintptr_t id) const {
  auto it = m_Textures.find(id);
  if (it == m_Textures.end()) return std::string();
  return std::to_string(it->second->w) + "x" + std::to_string(it->second->h) +
         " RGBA8 (software)";
}

/* ------------------------------------------------------------------------ */
/* Render targets                                                           */
/* ------------------------------------------------------------------------ */
uintptr_t RageDisplay_Software::CreateRenderTarget(
    const RenderTargetParam& param, int& iTextureWidthOut,
    int& iTextureHeightOut) {
  if (param.iWidth <= 0 || param.iHeight <= 0) return 0;
  auto tex = std::make_unique<SwTexture>();
  tex->Resize(param.iWidth, param.iHeight, param.bWithDepthBuffer);
  tex->filter = true;
  tex->wrap = false;
  iTextureWidthOut = tex->w;  // no power-of-two padding needed
  iTextureHeightOut = tex->h;
  uintptr_t h = m_NextHandle++;
  m_Textures[h] = std::move(tex);
  return h;
}

void RageDisplay_Software::SetRenderTarget(
    uintptr_t iHandle, bool bPreserveTexture) {
  if (iHandle == 0) {
    m_pTarget = nullptr;
    m_hTarget = 0;
    return;
  }
  auto it = m_Textures.find(iHandle);
  if (it == m_Textures.end()) return;
  m_pTarget = it->second.get();
  m_hTarget = iHandle;
  if (!bPreserveTexture || !m_pTarget->initialized) {
    std::fill(m_pTarget->color.begin(), m_pTarget->color.end(), 0u);
    std::fill(m_pTarget->depth.begin(), m_pTarget->depth.end(), 1.0f);
  }
  m_pTarget->initialized = true;
}

RageSurface* RageDisplay_Software::CreateScreenshot() {
  const SwSurface& s = m_Back;
  /* 32bpp, no alpha: R in the top byte. */
  RageSurface* out = CreateSurface(s.w, s.h, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0u);
  for (int y = 0; y < s.h; ++y) {
    uint32_t* dst = (uint32_t*)(out->pixels + (size_t)y * out->pitch);
    for (int x = 0; x < s.w; ++x) {
      uint32_t p = s.color[(size_t)y * s.w + x];
      dst[x] = ((p & 0xFF) << 24) | (((p >> 8) & 0xFF) << 16) |
               (((p >> 16) & 0xFF) << 8);
    }
  }
  return out;
}

/* ------------------------------------------------------------------------ */
/* Geometry creation                                                        */
/* ------------------------------------------------------------------------ */
RageCompiledGeometry* RageDisplay_Software::CreateCompiledGeometry() {
  return new RageCompiledGeometrySoftware(this);
}

void RageDisplay_Software::DeleteCompiledGeometry(RageCompiledGeometry* p) {
  delete p;
}

/* ------------------------------------------------------------------------ */
/* Vertex stage                                                             */
/* ------------------------------------------------------------------------ */
namespace {

void Transform4(const float* m, const float in[4], float out[4]) {
  for (int row = 0; row < 4; ++row) {
    out[row] = m[row] * in[0] + m[4 + row] * in[1] + m[8 + row] * in[2] +
               m[12 + row] * in[3];
  }
}

float Dot3(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Normalize3(float value[3]) {
  const float lengthSquared = Dot3(value, value);
  if (lengthSquared <= 1e-12f) {
    value[0] = value[1] = 0.0f;
    value[2] = 1.0f;
    return;
  }
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  for (int i = 0; i < 3; ++i) {
    value[i] *= inverseLength;
  }
}

float ClipDistance(const float position[4], int plane) {
  switch (plane) {
    case 0: return position[0] + position[3];
    case 1: return position[3] - position[0];
    case 2: return position[1] + position[3];
    case 3: return position[3] - position[1];
    case 4: return position[2] + position[3];
    default: return position[3] - position[2];
  }
}

}  // namespace

void RageDisplay_Software::PrepareDraw() {
  RageMatrix modelView;
  RageMatrixMultiply(&modelView, GetViewTop(), GetWorldTop());

  RageMatrix projection;
  RageMatrixMultiply(&projection, GetCentering(), GetProjectionTop());

  RageMatrix mvp;
  RageMatrixMultiply(&mvp, &modelView, &projection);

  std::memcpy(m_Ctx.modelView.m, static_cast<const float*>(modelView),
              sizeof(m_Ctx.modelView.m));
  std::memcpy(m_Ctx.mvp.m, static_cast<const float*>(mvp),
              sizeof(m_Ctx.mvp.m));
  std::memcpy(m_Ctx.texture.m, static_cast<const float*>(*GetTextureTop()),
              sizeof(m_Ctx.texture.m));

  const float a00 = modelView.m[0][0];
  const float a01 = modelView.m[1][0];
  const float a02 = modelView.m[2][0];
  const float a10 = modelView.m[0][1];
  const float a11 = modelView.m[1][1];
  const float a12 = modelView.m[2][1];
  const float a20 = modelView.m[0][2];
  const float a21 = modelView.m[1][2];
  const float a22 = modelView.m[2][2];
  const float cofactors[9] = {
      a11 * a22 - a12 * a21, a12 * a20 - a10 * a22,
      a10 * a21 - a11 * a20, a02 * a21 - a01 * a22,
      a00 * a22 - a02 * a20, a01 * a20 - a00 * a21,
      a01 * a12 - a02 * a11, a02 * a10 - a00 * a12,
      a00 * a11 - a01 * a10};
  const float determinant =
      a00 * cofactors[0] + a01 * cofactors[1] + a02 * cofactors[2];
  if (std::abs(determinant) > 1e-12f) {
    for (int component = 0; component < 9; ++component) {
      m_Ctx.normalMat[component] = cofactors[component] / determinant;
    }
  } else {
    const float linearTransform[9] = {a00, a01, a02, a10, a11,
                                      a12, a20, a21, a22};
    std::copy(std::begin(linearTransform), std::end(linearTransform),
              m_Ctx.normalMat);
  }
}

void RageDisplay_Software::ShadeVertex(
    const float p[3], const float n[3], const float col[4], const float uv[2],
  ClipVertex& out, const float textureScale[2]) const {
  const float position[4] = {p[0], p[1], p[2], 1.0f};
  Transform4(m_Ctx.mvp.m, position, out.c);

  float normal[3] = {
      m_Ctx.normalMat[0] * n[0] + m_Ctx.normalMat[1] * n[1] +
          m_Ctx.normalMat[2] * n[2],
      m_Ctx.normalMat[3] * n[0] + m_Ctx.normalMat[4] * n[1] +
          m_Ctx.normalMat[5] * n[2],
      m_Ctx.normalMat[6] * n[0] + m_Ctx.normalMat[7] * n[1] +
          m_Ctx.normalMat[8] * n[2]};
  Normalize3(normal);

  float lit[4] = {m_Material.emissive[0], m_Material.emissive[1],
                  m_Material.emissive[2], m_Material.diffuse[3]};
  if (m_bLighting) {
    for (int channel = 0; channel < 3; ++channel) {
      lit[channel] += m_Material.ambient[channel] * m_SceneAmbient[channel];
    }
    for (const Light& light : m_Lights) {
      if (!light.enabled) {
        continue;
      }

      const float diffuse = std::max(0.0f, Dot3(normal, light.toLight));
      float halfVector[3] = {light.toLight[0], light.toLight[1],
                             light.toLight[2] + 1.0f};
      Normalize3(halfVector);
      const float specular = m_Material.shininess > 0.0f
                                 ? std::pow(
                                       std::max(0.0f, Dot3(normal, halfVector)),
                                       m_Material.shininess)
                                 : 0.0f;
      for (int channel = 0; channel < 3; ++channel) {
        lit[channel] +=
            m_Material.ambient[channel] * light.ambient[channel] +
            m_Material.diffuse[channel] * light.diffuse[channel] * diffuse +
            m_Material.specular[channel] * light.specular[channel] * specular;
      }
    }
    for (int channel = 0; channel < 3; ++channel) {
      out.a[channel] = lit[channel] * col[channel];
    }
    out.a[3] = lit[3] * col[3];
  } else {
    std::copy(col, col + 4, out.a);
  }

  const float texCoord[4] = {uv[0], uv[1], 0.0f, 1.0f};
  float transformedTexCoord[4];
  Transform4(m_Ctx.texture.m, texCoord, transformedTexCoord);
  const float q = std::abs(transformedTexCoord[3]) > 1e-8f
                      ? transformedTexCoord[3]
                      : 1.0f;
  float u = transformedTexCoord[0] / q;
  float v = transformedTexCoord[1] / q;
  if (textureScale != nullptr) {
    u = u * textureScale[0] + uv[0] * (1.0f - textureScale[0]);
    v = v * textureScale[1] + uv[1] * (1.0f - textureScale[1]);
  }
  out.a[4] = out.a[6] = u;
  out.a[5] = out.a[7] = v;
}

void RageDisplay_Software::ShadeSpriteVertices(const RageSpriteVertex* v, int n) {
  m_Shaded.resize(n);
  for (int i = 0; i < n; ++i) {
    const float color[4] = {v[i].c.r / 255.0f, v[i].c.g / 255.0f,
                            v[i].c.b / 255.0f, v[i].c.a / 255.0f};
    ShadeVertex(v[i].p, v[i].n, color, v[i].t, m_Shaded[i]);
  }
}

void RageDisplay_Software::SubmitTriangle(
    const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
  std::array<ClipVertex, 16> polygon{};
  std::array<ClipVertex, 16> clipped{};
  polygon[0] = a;
  polygon[1] = b;
  polygon[2] = c;
  size_t polygonSize = 3;
  for (int plane = 0; plane < 6 && polygonSize > 0; ++plane) {
    size_t clippedSize = 0;
    ClipVertex previous = polygon[polygonSize - 1];
    float previousDistance = ClipDistance(previous.c, plane);
    for (size_t i = 0; i < polygonSize; ++i) {
      const ClipVertex& current = polygon[i];
      const float currentDistance = ClipDistance(current.c, plane);
      const bool previousInside = previousDistance >= 0.0f;
      const bool currentInside = currentDistance >= 0.0f;
      if (previousInside != currentInside) {
        const float t = previousDistance / (previousDistance - currentDistance);
        ClipVertex intersection{};
        for (int component = 0; component < 4; ++component) {
          intersection.c[component] = previous.c[component] +
                                      (current.c[component] -
                                       previous.c[component]) *
                                          t;
        }
        for (int component = 0; component < kAttr; ++component) {
          intersection.a[component] = previous.a[component] +
                                      (current.a[component] -
                                       previous.a[component]) *
                                          t;
        }
        if (clippedSize == clipped.size()) {
          return;
        }
        clipped[clippedSize++] = intersection;
      }
      if (currentInside) {
        if (clippedSize == clipped.size()) {
          return;
        }
        clipped[clippedSize++] = current;
      }
      previous = current;
      previousDistance = currentDistance;
    }
    polygon.swap(clipped);
    polygonSize = clippedSize;
  }

  if (polygonSize < 3) {
    return;
  }

  auto toRasterVertex = [this](const ClipVertex& vertex, RasterVertex& out) {
    const float w = vertex.c[3];
    if (w <= 1e-8f) {
      return false;
    }
    const float inverseW = 1.0f / w;
    const float x = vertex.c[0] * inverseW;
    const float y = vertex.c[1] * inverseW;
    const float z = vertex.c[2] * inverseW;
    const SwSurface& target = Target();
    out.x = (x * 0.5f + 0.5f) * target.w;
    out.y = (0.5f - y * 0.5f) * target.h;
    out.z = (z * 0.5f + 0.5f) * (m_ZFar - m_ZNear) + m_ZNear;
    out.invw = inverseW;
    std::copy(vertex.a, vertex.a + kAttr, out.a);
    return true;
  };

  RasterVertex first{};
  if (!toRasterVertex(polygon[0], first)) {
    return;
  }
  for (size_t i = 1; i + 1 < polygonSize; ++i) {
    RasterVertex second{}, third{};
    if (toRasterVertex(polygon[i], second) &&
        toRasterVertex(polygon[i + 1], third)) {
      RasterTriangle(
          first, second, third,
          m_pUnitTex[0] != nullptr || m_pUnitTex[1] != nullptr);
    }
  }
}

void RageDisplay_Software::RasterTriangle(
    RasterVertex v0, RasterVertex v1, RasterVertex v2, bool textured) {
  const auto edge = [](const RasterVertex& a, const RasterVertex& b, float x,
                       float y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
  };
  const float area = edge(v0, v1, v2.x, v2.y);
  if (std::abs(area) < 1e-8f) {
    return;
  }

  const bool frontFacing = area < 0.0f;
  if ((m_Cull == CULL_BACK && !frontFacing) ||
      (m_Cull == CULL_FRONT && frontFacing)) {
    return;
  }
  if (m_PolyMode == POLYGON_LINE) {
    DrawScreenLine(v0, v1, m_fLineWidth);
    DrawScreenLine(v1, v2, m_fLineWidth);
    DrawScreenLine(v2, v0, m_fLineWidth);
    return;
  }

  SwSurface& target = Target();
  const int minX = std::max(
      0, FloorToInt(std::min({v0.x, v1.x, v2.x})));
  const int minY = std::max(
      0, FloorToInt(std::min({v0.y, v1.y, v2.y})));
  const int maxX = std::min(
      target.w - 1, FloorToInt(std::max({v0.x, v1.x, v2.x})));
  const int maxY = std::min(
      target.h - 1, FloorToInt(std::max({v0.y, v1.y, v2.y})));
  if (minX > maxX || minY > maxY) {
    return;
  }

  const bool standardAlphaBlend =
      m_Blend.eq == SwBlendEq::Add &&
      m_Blend.src == SwBlendFactor::SrcAlpha &&
      m_Blend.dst == SwBlendFactor::OneMinusSrcAlpha &&
      m_Blend.separateAlpha;
  auto blendPixel = [this, standardAlphaBlend](float source[4],
                                               uint32_t destination) {
    if (standardAlphaBlend) {
      if (source[3] >= 1.0f) {
        return Pack(source);
      }
      if (source[3] <= 0.0f) {
        return destination;
      }
      float dest[4];
      Unpack(destination, dest);
      const float inverseAlpha = 1.0f - source[3];
      float result[4];
      for (int channel = 0; channel < 3; ++channel) {
        result[channel] =
            source[channel] * source[3] + dest[channel] * inverseAlpha;
      }
      result[3] = source[3] + dest[3] * inverseAlpha;
      return Pack(result);
    }

    float dest[4];
    Unpack(destination, dest);
    float result[4];
    for (int channel = 0; channel < 4; ++channel) {
      float sourceFactor = BlendFactor(
          m_Blend.src, source, dest, channel);
      float destinationFactor = BlendFactor(
          m_Blend.dst, source, dest, channel);
      if (channel == 3 && m_Blend.separateAlpha) {
        sourceFactor = 1.0f;
        destinationFactor = 1.0f - source[3];
      }
      result[channel] = Clamp01(Combine(
          m_Blend.eq, source[channel], sourceFactor, dest[channel],
          destinationFactor));
    }
    return Pack(result);
  };

  const float inverseArea = 1.0f / area;
  const float sampleX = minX + 0.5f;
  const float sampleY = minY + 0.5f;
  float rowWeight0 = edge(v1, v2, sampleX, sampleY) * inverseArea;
  float rowWeight1 = edge(v2, v0, sampleX, sampleY) * inverseArea;
  float rowWeight2 = edge(v0, v1, sampleX, sampleY) * inverseArea;
  const float weight0StepX = -(v2.y - v1.y) * inverseArea;
  const float weight1StepX = -(v0.y - v2.y) * inverseArea;
  const float weight2StepX = -(v1.y - v0.y) * inverseArea;
  const float weight0StepY = (v2.x - v1.x) * inverseArea;
  const float weight1StepY = (v0.x - v2.x) * inverseArea;
  const float weight2StepY = (v1.x - v0.x) * inverseArea;
  for (int y = minY; y <= maxY; ++y) {
    float weight0 = rowWeight0;
    float weight1 = rowWeight1;
    float weight2 = rowWeight2;
    for (int x = minX; x <= maxX; ++x) {
      if (weight0 < 0.0f || weight1 < 0.0f || weight2 < 0.0f) {
        weight0 += weight0StepX;
        weight1 += weight1StepX;
        weight2 += weight2StepX;
        continue;
      }

      const float depth =
          weight0 * v0.z + weight1 * v1.z + weight2 * v2.z;
      const size_t pixelIndex = static_cast<size_t>(y) * target.w + x;
      if (depthTest) {
        const float oldDepth = target.depth[pixelIndex];
        const bool depthPass = m_ZTest == ZTEST_WRITE_ON_PASS
                                   ? depth <= oldDepth
                                   : depth > oldDepth;
        if (!depthPass) {
          continue;
        }
      }

      const float reciprocalW = weight0 * v0.invw + weight1 * v1.invw +
                                weight2 * v2.invw;
      if (reciprocalW <= 1e-12f) {
        weight0 += weight0StepX;
        weight1 += weight1StepX;
        weight2 += weight2StepX;
        continue;
      }
      const float inverseReciprocalW = 1.0f / reciprocalW;
      const float p0 = weight0 * v0.invw * inverseReciprocalW;
      const float p1 = weight1 * v1.invw * inverseReciprocalW;
      const float p2 = weight2 * v2.invw * inverseReciprocalW;
      float color[4];
      for (int channel = 0; channel < 4; ++channel) {
        color[channel] = p0 * v0.a[channel] + p1 * v1.a[channel] +
                         p2 * v2.a[channel];
      }

      if (textured) {
        for (int unit = 0; unit < kNumUnits; ++unit) {
          const SwTexture* texture = m_pUnitTex[unit];
          if (texture == nullptr) {
            continue;
          }
          float sample[4];
          const int uvOffset = 4 + unit * 2;
          const float u = p0 * v0.a[uvOffset] + p1 * v1.a[uvOffset] +
                          p2 * v2.a[uvOffset];
          const float v = p0 * v0.a[uvOffset + 1] +
                          p1 * v1.a[uvOffset + 1] +
                          p2 * v2.a[uvOffset + 1];
          SampleTexture(*texture, u, v, sample);
          switch (m_UnitMode[unit]) {
            case TextureMode_Modulate:
              for (int channel = 0; channel < 4; ++channel) {
                color[channel] *= sample[channel];
              }
              break;
            case TextureMode_Glow:
              color[0] = color[1] = color[2] = 1.0f;
              color[3] *= sample[3];
              break;
            case TextureMode_Add:
              for (int channel = 0; channel < 4; ++channel) {
                color[channel] += sample[channel];
              }
              break;
            default:
              break;
          }
        }
      }

      if (m_bAlphaTest && color[3] <= 1.0f / 256.0f) {
        continue;
      }

      target.color[pixelIndex] = blendPixel(color, target.color[pixelIndex]);
      if (depthWrite) {
        target.depth[pixelIndex] = depth;
      }

      weight0 += weight0StepX;
      weight1 += weight1StepX;
      weight2 += weight2StepX;
    }
    rowWeight0 += weight0StepY;
    rowWeight1 += weight1StepY;
    rowWeight2 += weight2StepY;
  }
}

void RageDisplay_Software::DrawScreenLine(
    const RasterVertex& a, const RasterVertex& b, float width) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const int steps = std::max(1, FloorToInt(std::ceil(std::max(
                                  std::abs(dx), std::abs(dy)))));
  const int radius = std::max(0, FloorToInt(width * 0.5f));
  SwSurface& target = Target();

  for (int step = 0; step <= steps; ++step) {
    const float t = static_cast<float>(step) / steps;
    const float x = a.x + dx * t;
    const float y = a.y + dy * t;
    const float inverseW = a.invw + (b.invw - a.invw) * t;
    if (inverseW <= 1e-12f) {
      continue;
    }
    const float p = (1.0f - t) * a.invw / inverseW;
    const float q = t * b.invw / inverseW;
    float color[4];
    for (int channel = 0; channel < 4; ++channel) {
      color[channel] = p * a.a[channel] + q * b.a[channel];
    }
    const float depth = a.z + (b.z - a.z) * t;
    const int centerX = FloorToInt(x);
    const int centerY = FloorToInt(y);
    for (int offsetY = -radius; offsetY <= radius; ++offsetY) {
      for (int offsetX = -radius; offsetX <= radius; ++offsetX) {
        const int pixelX = centerX + offsetX;
        const int pixelY = centerY + offsetY;
        if (pixelX < 0 || pixelX >= target.w || pixelY < 0 ||
            pixelY >= target.h) {
          continue;
        }
        const size_t index = static_cast<size_t>(pixelY) * target.w + pixelX;
        if (!target.depth.empty() && m_ZTest != ZTEST_OFF) {
          const bool depthPass = m_ZTest == ZTEST_WRITE_ON_PASS
                                     ? depth <= target.depth[index]
                                     : depth > target.depth[index];
          if (!depthPass) {
            continue;
          }
        }
        if (m_bAlphaTest && color[3] <= 1.0f / 256.0f) {
          continue;
        }

        float destination[4];
        Unpack(target.color[index], destination);
        float result[4];
        for (int channel = 0; channel < 4; ++channel) {
          float sourceFactor =
              BlendFactor(m_Blend.src, color, destination, channel);
          float destinationFactor =
              BlendFactor(m_Blend.dst, color, destination, channel);
          if (channel == 3 && m_Blend.separateAlpha) {
            sourceFactor = 1.0f;
            destinationFactor = 1.0f - color[3];
          }
          result[channel] = Clamp01(Combine(
              m_Blend.eq, color[channel], sourceFactor, destination[channel],
              destinationFactor));
        }
        target.color[index] = Pack(result);
        if (!target.depth.empty() && m_bZWrite) {
          target.depth[index] = depth;
        }
      }
    }
  }
}

void RageDisplay_Software::DrawSpriteIndexed(
    const RageSpriteVertex* vertices, int numVertices,
    const std::vector<uint32_t>& indices) {
  if (vertices == nullptr || numVertices <= 0) {
    return;
  }
  PrepareDraw();
  ShadeSpriteVertices(vertices, numVertices);
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    const uint32_t i0 = indices[i];
    const uint32_t i1 = indices[i + 1];
    const uint32_t i2 = indices[i + 2];
    if (i0 >= m_Shaded.size() || i1 >= m_Shaded.size() ||
        i2 >= m_Shaded.size()) {
      continue;
    }
    SubmitTriangle(m_Shaded[i0], m_Shaded[i1], m_Shaded[i2]);
  }
}

void RageDisplay_Software::DrawQuadsInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices / 4) * 6);
  for (int i = 0; i + 3 < numVertices; i += 4) {
    m_Idx.insert(m_Idx.end(), {static_cast<uint32_t>(i),
                               static_cast<uint32_t>(i + 1),
                               static_cast<uint32_t>(i + 2),
                               static_cast<uint32_t>(i + 2),
                               static_cast<uint32_t>(i + 1),
                               static_cast<uint32_t>(i + 3)});
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawQuadStripInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices - 2) * 3);
  for (int i = 0; i + 3 < numVertices; i += 2) {
    m_Idx.insert(m_Idx.end(), {static_cast<uint32_t>(i),
                               static_cast<uint32_t>(i + 1),
                               static_cast<uint32_t>(i + 2),
                               static_cast<uint32_t>(i + 2),
                               static_cast<uint32_t>(i + 1),
                               static_cast<uint32_t>(i + 3)});
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawFanInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices - 2) * 3);
  for (int i = 1; i + 1 < numVertices; ++i) {
    m_Idx.insert(m_Idx.end(), {0u, static_cast<uint32_t>(i),
                               static_cast<uint32_t>(i + 1)});
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawStripInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices - 2) * 3);
  for (int i = 0; i + 2 < numVertices; ++i) {
    if (i & 1) {
      m_Idx.insert(m_Idx.end(), {static_cast<uint32_t>(i + 1),
                                 static_cast<uint32_t>(i),
                                 static_cast<uint32_t>(i + 2)});
    } else {
      m_Idx.insert(m_Idx.end(), {static_cast<uint32_t>(i),
                                 static_cast<uint32_t>(i + 1),
                                 static_cast<uint32_t>(i + 2)});
    }
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawTrianglesInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices));
  for (int i = 0; i + 2 < numVertices; ++i) {
    m_Idx.push_back(static_cast<uint32_t>(i));
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawSymmetricQuadStripInternal(
    const RageSpriteVertex vertices[], int numVertices) {
  m_Idx.clear();
  const int numPieces = (numVertices - 3) / 3;
  m_Idx.reserve(static_cast<size_t>(numPieces) * 12);
  for (int i = 0; i < numPieces; ++i) {
    const uint32_t start = static_cast<uint32_t>(i * 3);
    m_Idx.insert(m_Idx.end(), {start + 1, start + 3, start,
                               start + 1, start + 4, start + 3,
                               start + 1, start + 5, start + 4,
                               start + 1, start + 2, start + 5});
  }
  DrawSpriteIndexed(vertices, numVertices, m_Idx);
}

void RageDisplay_Software::DrawCompiledGeometryInternal(
    const RageCompiledGeometry* geometry, int meshIndex) {
  const auto* softwareGeometry =
      dynamic_cast<const RageCompiledGeometrySoftware*>(geometry);
  if (softwareGeometry != nullptr) {
    DrawMesh(softwareGeometry, meshIndex);
  }
}

void RageDisplay_Software::DrawMesh(
    const RageCompiledGeometrySoftware* geometry, int meshIndex) {
  if (geometry == nullptr || meshIndex < 0) {
    return;
  }
  const auto& mesh = geometry->Info(meshIndex);
  const size_t firstIndex = static_cast<size_t>(mesh.iTriangleStart) * 3;
  const size_t endIndex =
      firstIndex + static_cast<size_t>(mesh.iTriangleCount) * 3;
  PrepareDraw();
  const float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  for (size_t index = firstIndex; index + 2 < endIndex; index += 3) {
    ClipVertex triangle[3]{};
    for (int corner = 0; corner < 3; ++corner) {
      const RageModelVertex& vertex =
          geometry->Vertex(geometry->Indices()[index + corner]);
        const float uv[2] = {vertex.t.x, vertex.t.y};
        const float textureScale[2] = {vertex.TextureMatrixScale.x,
                       vertex.TextureMatrixScale.y};
        ShadeVertex(
          vertex.p, vertex.n, color, uv, triangle[corner], textureScale);
    }
    SubmitTriangle(triangle[0], triangle[1], triangle[2]);
  }
}
