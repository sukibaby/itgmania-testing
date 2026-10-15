#include "RageDisplay_Software.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
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
double NowMs();
}  // namespace

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
  m_Tris.clear();
  m_States.clear();
  m_pPendingTarget = nullptr;
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
    m_Tris.clear();
    m_States.clear();
    m_pPendingTarget = nullptr;
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
std::string RageDisplay_Software::GetStats() const {
  const FrameStats& f = m_Last;
  char buf[320];
  snprintf(
      buf, sizeof(buf),
      "\nSW tris sub/clip/cull/queued: %d/%d/%d/%d  px tested/written: %lld/%lld"
      "\nSW ms: render %.1f (raster %.1f in %d flush, clear %.1f)  present %.1f  wait %.1f  threads %d",
      f.trianglesSubmitted, f.trianglesClipped, f.trianglesCulled,
      f.trianglesRastered, f.pixelsTested, f.pixelsWritten, f.msRender, f.msFlush,
      f.flushes, f.msClear, f.msPresent, f.msLimiter, m_iThreads);
  return RageDisplay::GetStats() + buf;
}

void RageDisplay_Software::ResetStats() {
  m_Stats = FrameStats();
  RageDisplay::ResetStats();
}

bool RageDisplay_Software::BeginFrame() {
  Flush();
  m_Stats = FrameStats();
  m_dFrameStartMs = NowMs();
  m_pTarget = nullptr;
  m_hTarget = 0;
  const double t0 = NowMs();
  std::fill(m_Back.color.begin(), m_Back.color.end(), 0xFF000000u);
  std::fill(m_Back.depth.begin(), m_Back.depth.end(), 1.0f);
  m_Back.initialized = true;
  m_Stats.msClear = NowMs() - t0;
  return RageDisplay::BeginFrame();
}

void RageDisplay_Software::EndFrame() {
  m_pTarget = nullptr;
  m_hTarget = 0;
  Flush();
  const double tRendered = NowMs();
  m_Stats.msRender = tRendered - m_dFrameStartMs;

  FrameLimitBeforeVsync();
  const double tPresent = NowMs();
  if (m_pPresenter) {
    m_pPresenter->Update();
    m_pPresenter->Present(
        m_Back.color.data(), m_Back.w, m_Back.h, m_Back.w * (int)sizeof(uint32_t));
  }
  m_Stats.msPresent = NowMs() - tPresent;
  const double tLimiter = NowMs();
  FrameLimitAfterVsync();
  m_Stats.msLimiter = NowMs() - tLimiter + (tPresent - tRendered);
  m_Last = m_Stats;
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
  Flush();
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
  Flush();  // queued triangles may still sample the old texels
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
  Flush();
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
  Flush();
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
  Flush();  // finish the previous target before switching
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
  Flush();
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


SwMat4 LoadGL(const RageMatrix* matrix) {
  SwMat4 out;
  std::memcpy(out.m, matrix, sizeof(out.m));
  return out;
}

/* a * b, column-vector convention, column-major storage (glMultMatrix). */
SwMat4 MulGL(const SwMat4& a, const SwMat4& b) {
  SwMat4 out;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
      out.m[col * 4 + row] = sum;
    }
  }
  return out;
}

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
  /* Mirror RageDisplay_Legacy::SendCurrentMatrices exactly:
   *   GL_PROJECTION = Centering * ProjectionTop
   *   GL_MODELVIEW  = ViewTop * WorldTop
   * and clip = Projection * ModelView * v (column vectors, column-major
   * memory). Done locally so we don't depend on RageMatrixMultiply's argument
   * convention. */
  const Mat4 centering = LoadGL(GetCentering());
  const Mat4 projectionTop = LoadGL(GetProjectionTop());
  const Mat4 view = LoadGL(GetViewTop());
  const Mat4 world = LoadGL(GetWorldTop());
  const Mat4 projection = MulGL(centering, projectionTop);
  m_Ctx.modelView = MulGL(view, world);
  m_Ctx.mvp = MulGL(projection, m_Ctx.modelView);
  m_Ctx.texture = LoadGL(GetTextureTop());

  const float* modelViewMemory = m_Ctx.modelView.m;
  struct {
    float m[4][4];
  } modelView;
  std::memcpy(modelView.m, modelViewMemory, sizeof(modelView.m));

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

/* ------------------------------------------------------------------------ */
/* Triangle submission: clip -> screen space -> queue                        */
/* ------------------------------------------------------------------------ */
namespace {

inline unsigned OutCode(const float c[4]) {
  unsigned code = 0;
  for (int plane = 0; plane < 6; ++plane) {
    if (ClipDistance(c, plane) < 0.0f) code |= 1u << plane;
  }
  return code;
}

}  // namespace

void RageDisplay_Software::SubmitTriangle(
    const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
  ++m_Stats.trianglesSubmitted;

  const unsigned codeA = OutCode(a.c), codeB = OutCode(b.c), codeC = OutCode(c.c);
  if (codeA & codeB & codeC) {  // entirely outside one plane
    ++m_Stats.trianglesClipped;
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

  if ((codeA | codeB | codeC) == 0) {  // fully inside: no clipping needed
    RasterVertex r0, r1, r2;
    if (toRasterVertex(a, r0) && toRasterVertex(b, r1) && toRasterVertex(c, r2)) {
      QueueTriangle(r0, r1, r2);
    }
    return;
  }

  std::array<ClipVertex, 16> polygon;
  std::array<ClipVertex, 16> clipped;
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
        ClipVertex intersection;
        for (int component = 0; component < 4; ++component) {
          intersection.c[component] =
              previous.c[component] +
              (current.c[component] - previous.c[component]) * t;
        }
        for (int component = 0; component < kAttr; ++component) {
          intersection.a[component] =
              previous.a[component] +
              (current.a[component] - previous.a[component]) * t;
        }
        if (clippedSize == clipped.size()) return;
        clipped[clippedSize++] = intersection;
      }
      if (currentInside) {
        if (clippedSize == clipped.size()) return;
        clipped[clippedSize++] = current;
      }
      previous = current;
      previousDistance = currentDistance;
    }
    for (size_t i = 0; i < clippedSize; ++i) polygon[i] = clipped[i];
    polygonSize = clippedSize;
  }

  if (polygonSize < 3) {
    ++m_Stats.trianglesClipped;
    return;
  }

  RasterVertex first;
  if (!toRasterVertex(polygon[0], first)) return;
  for (size_t i = 1; i + 1 < polygonSize; ++i) {
    RasterVertex second, third;
    if (toRasterVertex(polygon[i], second) &&
        toRasterVertex(polygon[i + 1], third)) {
      QueueTriangle(first, second, third);
    }
  }
}

namespace {

bool SameState(const SwState& a, const SwState& b) {
  if (a.blend.src != b.blend.src || a.blend.dst != b.blend.dst ||
      a.blend.eq != b.blend.eq || a.blend.separateAlpha != b.blend.separateAlpha ||
      a.zwrite != b.zwrite || a.ztest != b.ztest || a.alphaTest != b.alphaTest) {
    return false;
  }
  for (int u = 0; u < 2; ++u) {
    if (a.tex[u] != b.tex[u] || a.mode[u] != b.mode[u] || a.wrap[u] != b.wrap[u] ||
        a.filter[u] != b.filter[u]) {
      return false;
    }
  }
  return true;
}

uint32_t PackColor8(float r, float g, float b, float a) {
  auto q = [](float x) {
    int v = (int)(x * 255.0f + 0.5f);
    return (uint32_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
  };
  return q(r) | (q(g) << 8) | (q(b) << 16) | (q(a) << 24);
}

}  // namespace

void RageDisplay_Software::QueueTriangle(
    RasterVertex v0, RasterVertex v1, RasterVertex v2) {
  const auto edge = [](const RasterVertex& a, const RasterVertex& b, float x, float y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
  };
  const float area = edge(v0, v1, v2.x, v2.y);
  if (std::abs(area) < 1e-8f) {
    return;
  }

  const bool frontFacing = area < 0.0f;
  if ((m_Cull == CULL_BACK && !frontFacing) ||
      (m_Cull == CULL_FRONT && frontFacing)) {
    ++m_Stats.trianglesCulled;
    return;
  }
  if (m_PolyMode == POLYGON_LINE) {
    DrawScreenLine(v0, v1, m_fLineWidth);
    DrawScreenLine(v1, v2, m_fLineWidth);
    DrawScreenLine(v2, v0, m_fLineWidth);
    return;
  }

  SwSurface& target = Target();
  const int minX = std::max(0, FloorToInt(std::min({v0.x, v1.x, v2.x})));
  const int minY = std::max(0, FloorToInt(std::min({v0.y, v1.y, v2.y})));
  const int maxX = std::min(target.w - 1, FloorToInt(std::max({v0.x, v1.x, v2.x})));
  const int maxY = std::min(target.h - 1, FloorToInt(std::max({v0.y, v1.y, v2.y})));
  if (minX > maxX || minY > maxY) {
    return;
  }
  ++m_Stats.trianglesRastered;

  // Pending triangles all belong to one target.
  if (!m_Tris.empty() && m_pPendingTarget != &target) Flush();
  m_pPendingTarget = &target;

  // Capture the live state (merge with the previous batch if identical).
  SwState st;
  st.blend = m_Blend;
  st.zwrite = m_bZWrite;
  st.ztest = m_ZTest;
  st.alphaTest = m_bAlphaTest;
  for (int u = 0; u < kNumUnits; ++u) {
    st.tex[u] = m_pUnitTex[u];
    st.mode[u] = m_UnitMode[u];
    if (m_pUnitTex[u]) {
      st.wrap[u] = m_pUnitTex[u]->wrap;
      st.filter[u] = m_pUnitTex[u]->filter;
    }
  }
  using F = SwBlendFactor;
  if (m_Blend.eq == SwBlendEq::Add) {
    if (m_Blend.src == F::SrcAlpha && m_Blend.dst == F::OneMinusSrcAlpha &&
        m_Blend.separateAlpha) {
      st.blendKind = 1;
    } else if (m_Blend.src == F::SrcAlpha && m_Blend.dst == F::One &&
               !m_Blend.separateAlpha) {
      st.blendKind = 2;
    } else if (m_Blend.src == F::One && m_Blend.dst == F::Zero &&
               !m_Blend.separateAlpha) {
      st.blendKind = 3;
    }
  }
  if (m_States.empty() || !SameState(m_States.back(), st)) m_States.push_back(st);

  SwTri t;
  t.x[0] = v0.x; t.x[1] = v1.x; t.x[2] = v2.x;
  t.y[0] = v0.y; t.y[1] = v1.y; t.y[2] = v2.y;
  t.invArea = 1.0f / area;
  t.orient = area > 0.0f ? 1.0f : -1.0f;
  const RasterVertex* v[3] = {&v0, &v1, &v2};
  const auto topLeft = [&](int a, int b) {
    const float dx = (v[b]->x - v[a]->x) * t.orient;
    const float dy = (v[b]->y - v[a]->y) * t.orient;
    return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
  };
  t.tl[0] = topLeft(1, 2);
  t.tl[1] = topLeft(2, 0);
  t.tl[2] = topLeft(0, 1);
  t.xmin = minX; t.xmax = maxX; t.ymin = minY; t.ymax = maxY;
  t.state = (uint32_t)(m_States.size() - 1);

  // Perspective-correct only when 1/w actually varies (it doesn't in 2D).
  const float wMin = std::min({v0.invw, v1.invw, v2.invw});
  const float wMax = std::max({v0.invw, v1.invw, v2.invw});
  t.affine = (wMax - wMin) <= 1e-6f * std::max(std::abs(wMax), 1e-12f);

  for (int i = 0; i < 3; ++i) {
    const float m = t.affine ? 1.0f : v[i]->invw;
    for (int k = 0; k < kAttr; ++k) t.f[k][i] = v[i]->a[k] * m;
    t.f[8][i] = v[i]->z;
    t.f[9][i] = v[i]->invw;
  }
  // Barycentric gradients: w0<-edge(v1,v2), w1<-edge(v2,v0), w2<-edge(v0,v1).
  const float dwdx[3] = {-(v2.y - v1.y) * t.invArea, -(v0.y - v2.y) * t.invArea,
                         -(v1.y - v0.y) * t.invArea};
  for (int k = 0; k < SwTri::kPlanes; ++k) {
    t.gx[k] = t.f[k][0] * dwdx[0] + t.f[k][1] * dwdx[1] + t.f[k][2] * dwdx[2];
  }

  t.constColor = true;
  for (int k = 0; k < 4; ++k) {
    if (v0.a[k] != v1.a[k] || v0.a[k] != v2.a[k]) t.constColor = false;
  }
  t.constColor8 = PackColor8(v0.a[0], v0.a[1], v0.a[2], v0.a[3]);

  m_Tris.push_back(t);
}

/* ------------------------------------------------------------------------ */
/* Band rasterizer + thread pool                                            */
/* ------------------------------------------------------------------------ */
namespace {

struct LocalStats {
  long long tested = 0, written = 0;
};

inline uint32_t Mul8(uint32_t x, uint32_t y) {
  const uint32_t t = x * y + 128u;
  return (t + (t >> 8)) >> 8;
}

/* Per-channel lerp of two packed RGBA8 values, t in [0,255]. */
inline uint32_t Lerp8(uint32_t a, uint32_t b, uint32_t t) {
  const uint32_t it = 256u - t;
  const uint32_t rb = (((a & 0x00FF00FFu) * it + (b & 0x00FF00FFu) * t) >> 8) & 0x00FF00FFu;
  const uint32_t ga = (((a >> 8) & 0x00FF00FFu) * it + ((b >> 8) & 0x00FF00FFu) * t) & 0xFF00FF00u;
  return rb | ga;
}

/* dst*(255-sa) + src*sa for all four channels at once; src alpha must be
 * forced to 255 so the alpha channel becomes sa + da*(1-sa). */
inline uint32_t BlendNormal(uint32_t src, uint32_t dst, uint32_t sa) {
  const uint32_t isa = 255u - sa;
  src |= 0xFF000000u;
  uint32_t rb = (src & 0x00FF00FFu) * sa + (dst & 0x00FF00FFu) * isa + 0x00800080u;
  uint32_t ga = ((src >> 8) & 0x00FF00FFu) * sa + ((dst >> 8) & 0x00FF00FFu) * isa + 0x00800080u;
  rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
  ga = ((ga + ((ga >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
  return rb | (ga << 8);
}

inline int FastFloor(float f) {
  if (!(f > -1e9f)) f = -1e9f;
  if (f > 1e9f) f = 1e9f;
  const int i = (int)f;
  return i - (f < (float)i);
}

inline int WrapCoord(int x, int n, bool wrap) {
  if ((unsigned)x < (unsigned)n) return x;
  if (wrap) {
    x %= n;
    return x < 0 ? x + n : x;
  }
  return x < 0 ? 0 : n - 1;
}

uint32_t SampleTexel(
    const SwTexture& t, bool wrap, bool filter, float u, float v) {
  const int W = t.w, H = t.h;
  const uint32_t* px = t.color.data();
  if (!filter) {
    const int x = WrapCoord(FastFloor(u * (float)W), W, wrap);
    const int y = WrapCoord(FastFloor(v * (float)H), H, wrap);
    return px[(size_t)y * W + x];
  }
  const float fu = u * (float)W - 0.5f, fv = v * (float)H - 0.5f;
  const int x0 = FastFloor(fu), y0 = FastFloor(fv);
  const uint32_t tx = std::min(255u, (uint32_t)((fu - (float)x0) * 256.0f));
  const uint32_t ty = std::min(255u, (uint32_t)((fv - (float)y0) * 256.0f));
  const int xa = WrapCoord(x0, W, wrap), xb = WrapCoord(x0 + 1, W, wrap);
  const int ya = WrapCoord(y0, H, wrap), yb = WrapCoord(y0 + 1, H, wrap);
  const uint32_t* r0 = px + (size_t)ya * W;
  const uint32_t top = tx ? Lerp8(r0[xa], r0[xb], tx) : r0[xa];
  if (!ty) return top;
  const uint32_t* r1 = px + (size_t)yb * W;
  const uint32_t bot = tx ? Lerp8(r1[xa], r1[xb], tx) : r1[xa];
  return Lerp8(top, bot, ty);
}

inline uint32_t ApplyTextureMode(TextureMode mode, uint32_t c, uint32_t t) {
  switch (mode) {
    case TextureMode_Modulate:
      if (c == 0xFFFFFFFFu) return t;
      if ((c & 0x00FFFFFFu) == 0x00FFFFFFu) {  // white tint, only alpha varies
        return (t & 0x00FFFFFFu) | (Mul8(t >> 24, c >> 24) << 24);
      }
      return Mul8(c & 255u, t & 255u) | (Mul8((c >> 8) & 255u, (t >> 8) & 255u) << 8) |
             (Mul8((c >> 16) & 255u, (t >> 16) & 255u) << 16) |
             (Mul8(c >> 24, t >> 24) << 24);
    case TextureMode_Glow:  // rgb from vertex color, alpha from texture
      return (c & 0x00FFFFFFu) | (Mul8(c >> 24, t >> 24) << 24);
    case TextureMode_Add: {  // rgb = vertex + texture (saturating), alpha modulated
      uint32_t out = 0;
      for (int s = 0; s < 24; s += 8) {
        out |= std::min(255u, ((c >> s) & 255u) + ((t >> s) & 255u)) << s;
      }
      return out | (Mul8(c >> 24, t >> 24) << 24);
    }
    default:
      return c;
  }
}

/* Blend colour c over dst according to the captured state. Returns false if
 * the pixel was skipped (nothing written). */
inline bool BlendPixel(const SwState& s, uint32_t& dst, uint32_t c) {
  const uint32_t sa = c >> 24;
  if (s.alphaTest && sa == 0) return false;
  switch (s.blendKind) {
    case 1:  // normal alpha
      if (sa == 255) {
        dst = c;
      } else if (sa == 0) {
        return false;
      } else {
        dst = BlendNormal(c, dst, sa);
      }
      return true;
    case 2: {  // additive: dst + src*alpha
      if (sa == 0) return false;
      uint32_t out = 0;
      for (int sh = 0; sh < 32; sh += 8) {
        out |= std::min(255u, ((dst >> sh) & 255u) + Mul8((c >> sh) & 255u, sa)) << sh;
      }
      dst = out;
      return true;
    }
    case 3:  // copy
      dst = c;
      return true;
    default: {  // generic (rare modes)
      float src[4], d[4], res[4];
      Unpack(c, src);
      Unpack(dst, d);
      for (int ch = 0; ch < 4; ++ch) {
        float sf = BlendFactor(s.blend.src, src, d, ch);
        float df = BlendFactor(s.blend.dst, src, d, ch);
        if (ch == 3 && s.blend.separateAlpha) {
          sf = 1.0f;
          df = 1.0f - src[3];
        }
        res[ch] = Clamp01(Combine(s.blend.eq, src[ch], sf, d[ch], df));
      }
      dst = Pack(res);
      return true;
    }
  }
}

/* Fast path: affine triangle, exactly one texture. Texture coordinates step in
 * 16.16 fixed point so the inner loop has no floor/clamp/float conversion. */
bool AffineTexturedSpan(
    const SwTri& t, const SwState& s, SwSurface& target, int y, int xlo, int xhi,
    const float* cur, LocalStats& ls, bool depthTest, bool depthWrite,
    bool writeOnPass) {
  const SwTexture& tex = *s.tex[0];
  const int W = tex.w, H = tex.h;
  const bool filter = s.filter[0], wrap = s.wrap[0];
  const float off = filter ? 0.5f : 0.0f;
  const float n = (float)(xhi - xlo);
  const float uTex0 = cur[4] * W - off, uTex1 = (cur[4] + t.gx[4] * n) * W - off;
  const float vTex0 = cur[5] * H - off, vTex1 = (cur[5] + t.gx[5] * n) * H - off;
  const float kLimit = 16000.0f;  // keep 16.16 inside int32
  if (std::abs(uTex0) > kLimit || std::abs(uTex1) > kLimit ||
      std::abs(vTex0) > kLimit || std::abs(vTex1) > kLimit) {
    return false;  // caller falls back to the generic loop
  }
  int U = (int)(uTex0 * 65536.0f), V = (int)(vTex0 * 65536.0f);
  const int dU = (int)(t.gx[4] * W * 65536.0f), dV = (int)(t.gx[5] * H * 65536.0f);
  float z = cur[8];
  const float dz = t.gx[8];
  float col[4] = {cur[0], cur[1], cur[2], cur[3]};
  const int width = target.w;
  uint32_t* colorRow = target.color.data() + (size_t)y * width;
  float* depthRow = depthTest ? target.depth.data() + (size_t)y * width : nullptr;
  const uint32_t* texels = tex.color.data();
  const TextureMode mode = s.mode[0];

  for (int x = xlo; x <= xhi; ++x, U += dU, V += dV, z += dz) {
    if (!t.constColor) {
      for (int k = 0; k < 4; ++k) col[k] += t.gx[k];
    }
    ++ls.tested;
    if (depthTest) {
      const float old = depthRow[x];
      if (writeOnPass ? !(z <= old) : !(z > old)) continue;
    }

    uint32_t texel;
    if (!filter) {
      int xi = U >> 16, yi = V >> 16;
      if ((unsigned)xi >= (unsigned)W) xi = WrapCoord(xi, W, wrap);
      if ((unsigned)yi >= (unsigned)H) yi = WrapCoord(yi, H, wrap);
      texel = texels[(size_t)yi * W + xi];
    } else {
      const int x0 = U >> 16, y0 = V >> 16;
      const uint32_t tx = (U >> 8) & 255, ty = (V >> 8) & 255;
      int xa = x0, xb = x0 + 1, ya = y0, yb = y0 + 1;
      if ((unsigned)x0 >= (unsigned)(W - 1)) {
        xa = WrapCoord(x0, W, wrap);
        xb = WrapCoord(x0 + 1, W, wrap);
      }
      if ((unsigned)y0 >= (unsigned)(H - 1)) {
        ya = WrapCoord(y0, H, wrap);
        yb = WrapCoord(y0 + 1, H, wrap);
      }
      const uint32_t* r0 = texels + (size_t)ya * W;
      const uint32_t top = tx ? Lerp8(r0[xa], r0[xb], tx) : r0[xa];
      if (!ty) {
        texel = top;
      } else {
        const uint32_t* r1 = texels + (size_t)yb * W;
        const uint32_t bot = tx ? Lerp8(r1[xa], r1[xb], tx) : r1[xa];
        texel = Lerp8(top, bot, ty);
      }
    }

    // Transparent texel under normal alpha: nothing to do, skip colour math.
    if (s.blendKind == 1 && (texel >> 24) == 0 && mode == TextureMode_Modulate) continue;

    const uint32_t c0 = t.constColor ? t.constColor8
                                     : PackColor8(col[0], col[1], col[2], col[3]);
    const uint32_t c = ApplyTextureMode(mode, c0, texel);
    if (BlendPixel(s, colorRow[x], c)) {
      ++ls.written;
      if (depthWrite) depthRow[x] = z;
    }
  }
  return true;
}

void RasterTriBand(
    const SwTri& t, const SwState& s, SwSurface& target, int y0, int y1,
    LocalStats& ls) {
  const int ys = std::max(t.ymin, y0), ye = std::min(t.ymax, y1 - 1);
  const bool depthTest = !target.depth.empty() && s.ztest != ZTEST_OFF;
  const bool depthWrite = depthTest && s.zwrite;
  const bool writeOnPass = s.ztest == ZTEST_WRITE_ON_PASS;
  const int W = target.w;

  for (int y = ys; y <= ye; ++y) {
    const float py = y + 0.5f;

    // Exact edge test (same arithmetic for bounds and interior => no cracks).
    const auto inside = [&](int x) {
      const float px = x + 0.5f;
      for (int i = 0; i < 3; ++i) {
        const int a = (i + 1) % 3, b = (i + 2) % 3;
        const float e = ((t.x[b] - t.x[a]) * (py - t.y[a]) -
                         (t.y[b] - t.y[a]) * (px - t.x[a])) * t.orient;
        if (e < 0.0f || (e == 0.0f && !t.tl[i])) return false;
      }
      return true;
    };

    // Analytic span bounds per edge, widened by a pixel, then trimmed exactly.
    int xlo = t.xmin, xhi = t.xmax;
    bool empty = false;
    for (int i = 0; i < 3 && !empty; ++i) {
      const int a = (i + 1) % 3, b = (i + 2) % 3;
      const float dxe = t.x[b] - t.x[a], dye = t.y[b] - t.y[a];
      const float c = -dye * t.orient;
      const float d = (dxe * (py - t.y[a]) + dye * t.x[a]) * t.orient;
      if (c > 1e-9f) {
        const float xb = std::min(std::max(-d / c - 0.5f, -2.0f), (float)W + 2.0f);
        xlo = std::max(xlo, FastFloor(xb) - 1);
      } else if (c < -1e-9f) {
        const float xb = std::min(std::max(-d / c - 0.5f, -2.0f), (float)W + 2.0f);
        xhi = std::min(xhi, FastFloor(xb) + 1);
      } else if (d < 0.0f || (d == 0.0f && !t.tl[i])) {
        empty = true;
      }
    }
    if (empty || xlo > xhi) continue;
    while (xlo <= xhi && !inside(xlo)) ++xlo;
    while (xhi >= xlo && !inside(xhi)) --xhi;
    if (xlo > xhi) continue;

    // Plane values at the span start.
    float cur[SwTri::kPlanes];
    {
      const float px = xlo + 0.5f;
      float w[3];
      for (int i = 0; i < 3; ++i) {
        const int a = (i + 1) % 3, b = (i + 2) % 3;
        w[i] = ((t.x[b] - t.x[a]) * (py - t.y[a]) - (t.y[b] - t.y[a]) * (px - t.x[a])) *
               t.invArea;
      }
      for (int k = 0; k < SwTri::kPlanes; ++k) {
        cur[k] = w[0] * t.f[k][0] + w[1] * t.f[k][1] + w[2] * t.f[k][2];
      }
    }

    uint32_t* colorRow = target.color.data() + (size_t)y * W;
    float* depthRow = depthTest ? target.depth.data() + (size_t)y * W : nullptr;

    if (t.affine && s.tex[0] && !s.tex[1] &&
        AffineTexturedSpan(
            t, s, target, y, xlo, xhi, cur, ls, depthTest, depthWrite,
            writeOnPass)) {
      continue;
    }

    for (int x = xlo; x <= xhi; ++x) {
      ++ls.tested;
      do {
        const float z = cur[8];
        if (depthTest) {
          const float old = depthRow[x];
          if (writeOnPass ? !(z <= old) : !(z > old)) break;
        }

        float q = 1.0f;
        if (!t.affine) {
          if (cur[9] <= 1e-12f) break;
          q = 1.0f / cur[9];
        }
        uint32_t c = t.constColor
                         ? t.constColor8
                         : PackColor8(cur[0] * q, cur[1] * q, cur[2] * q, cur[3] * q);

        for (int u = 0; u < 2; ++u) {
          if (!s.tex[u]) continue;
          const uint32_t texel = SampleTexel(
              *s.tex[u], s.wrap[u], s.filter[u], cur[4 + 2 * u] * q, cur[5 + 2 * u] * q);
          c = ApplyTextureMode(s.mode[u], c, texel);
        }

        if (BlendPixel(s, colorRow[x], c)) {
          ++ls.written;
          if (depthWrite) depthRow[x] = z;
        }
      } while (false);
      for (int k = 0; k < SwTri::kPlanes; ++k) cur[k] += t.gx[k];
    }
  }
}

}  // namespace

class SwThreadPool {
 public:
  explicit SwThreadPool(int threads) : m_n(std::max(1, threads)) {
    for (int i = 1; i < m_n; ++i) {
      m_workers.emplace_back([this] { Loop(); });
    }
  }
  ~SwThreadPool() {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_quit = true;
    }
    m_wake.notify_all();
    for (auto& w : m_workers) w.join();
  }
  int Size() const { return m_n; }

  /* Run fn on every thread (including the caller); returns when all finish. */
  void Run(const std::function<void()>& fn) {
    if (m_workers.empty()) {
      fn();
      return;
    }
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_job = &fn;
      ++m_generation;
      m_pending = (int)m_workers.size();
    }
    m_wake.notify_all();
    fn();
    std::unique_lock<std::mutex> lock(m_mutex);
    m_done.wait(lock, [this] { return m_pending == 0; });
  }

 private:
  void Loop() {
    uint64_t seen = 0;
    for (;;) {
      const std::function<void()>* job = nullptr;
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wake.wait(lock, [&] { return m_quit || m_generation != seen; });
        if (m_quit) return;
        seen = m_generation;
        job = m_job;
      }
      (*job)();
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (--m_pending == 0) m_done.notify_one();
      }
    }
  }

  int m_n;
  std::vector<std::thread> m_workers;
  std::mutex m_mutex;
  std::condition_variable m_wake, m_done;
  const std::function<void()>* m_job = nullptr;
  uint64_t m_generation = 0;
  int m_pending = 0;
  bool m_quit = false;
};

RageDisplay_Software::~RageDisplay_Software() {}

namespace {
double NowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

void RageDisplay_Software::Flush() {
  if (m_Tris.empty()) return;
  const double t0 = NowMs();
  SwSurface& target = *m_pPendingTarget;

  if (!m_pPool) {
    int threads = (int)std::thread::hardware_concurrency();
    threads = std::max(1, std::min(threads, 16));
    if (const char* env = std::getenv("SW_RENDER_THREADS")) {
      threads = std::max(1, std::min(atoi(env), 64));
    }
    m_pPool = std::make_unique<SwThreadPool>(threads);
    m_iThreads = threads;
  }

  constexpr int kBandRows = 16;
  const int bands = (target.h + kBandRows - 1) / kBandRows;
  std::atomic<int> nextBand{0};
  std::atomic<long long> tested{0}, written{0};
  const std::function<void()> work = [&] {
    LocalStats ls;
    for (int b = nextBand.fetch_add(1); b < bands; b = nextBand.fetch_add(1)) {
      const int y0 = b * kBandRows, y1 = std::min(target.h, y0 + kBandRows);
      for (const SwTri& tri : m_Tris) {  // submission order => correct blending
        if (tri.ymax < y0 || tri.ymin >= y1) continue;
        RasterTriBand(tri, m_States[tri.state], target, y0, y1, ls);
      }
    }
    tested += ls.tested;
    written += ls.written;
  };
  m_pPool->Run(work);

  m_Stats.pixelsTested += tested;
  m_Stats.pixelsWritten += written;
  ++m_Stats.flushes;
  m_Tris.clear();
  m_States.clear();
  m_pPendingTarget = nullptr;
  m_Stats.msFlush += NowMs() - t0;
}

void RageDisplay_Software::DrawScreenLine(
    const RasterVertex& a, const RasterVertex& b, float width) {
  Flush();  // lines are drawn immediately; keep submission order
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
        if (!target.depth.empty() && m_bZWrite && m_ZTest != ZTEST_OFF) {
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
  /* GL_QUADS: the four vertices run around the perimeter (TL, BL, BR, TR for
   * sprites), so split along the 0-2 diagonal. */
  m_Idx.clear();
  m_Idx.reserve(static_cast<size_t>(numVertices / 4) * 6);
  for (int i = 0; i + 3 < numVertices; i += 4) {
    const uint32_t b = static_cast<uint32_t>(i);
    m_Idx.insert(m_Idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
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
  for (int i = 0; i < numVertices; ++i) {
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
