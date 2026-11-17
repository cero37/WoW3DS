// fsr_scene.cpp — scene target + LITE/PICA-CAS post-processing for w3dproto.
#include "fsr_scene.hpp"

#include "vram_guard.hpp"

extern "C" {
#include "fsr3ds.h"
}
#include "fsr_blit_shbin.h"

#include <string.h>

namespace fsrScene {
namespace {

struct Vert {
    float x, y;
    float u0, v0;
    float u1, v1;
    float u2, v2;
    float r, g, b, a;
};

constexpr int kMaxTex = 1024;   // PICA limit; enough for 800x240 in a 1024x256 texture

shaderProgram_s s_prog;
DVLB_s* s_dvlb = nullptr;
int s_uProj = -1;
Vert* s_v = nullptr;

C3D_Tex s_tex;                    // scene colour (RGBA8, VRAM)
C3D_RenderTarget* s_target = nullptr;
bool s_texOk = false;

C3D_Tex s_texB;                   // CAS intermediate (RGBA8, VRAM), no depth
C3D_RenderTarget* s_targetB = nullptr;
bool s_texBOk = false;

int s_texW = 0, s_texH = 0;
int s_w = 0, s_h = 0;

C3D_Tex s_dummy;   // units 1/2 after the pass: never a deleted scene texture
bool s_dummyOk = false;

int pot(int v) {
    int p = 8;
    while (p < v && p < kMaxTex) p <<= 1;
    return p;
}

void dropTemp() {
    if (s_targetB) {
        C3D_RenderTargetDelete(s_targetB);
        s_targetB = nullptr;
    }
    if (s_texBOk) {
        C3D_TexDelete(&s_texB);
        s_texBOk = false;
    }
}

void dropTarget() {
    dropTemp();
    if (s_target) {
        C3D_RenderTargetDelete(s_target);
        s_target = nullptr;
    }
    if (s_texOk) {
        C3D_TexDelete(&s_tex);
        s_texOk = false;
    }
    s_texW = s_texH = 0;
    s_w = s_h = 0;
}

void bindTex(C3D_Tex* t) {
    C3D_TexSetFilter(t, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(t, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexBind(0, t);
    C3D_TexBind(1, t);
    C3D_TexBind(2, t);
}

// One full-screen blit. toScreen selects the tilted screen projection; an
// offscreen pass writes into the top s_h rows of the CAS intermediate target.
// du/dv are one source texel; only the axis a kernel uses may be non-zero.
// insetTexels keeps the centre samples (and their ±1 taps) away from the
// cleared unused part of the POT texture at the right/bottom edge. The CAS
// horizontal pass uses 0: its outer pixels are cropped by the vertical pass.
void drawQuad(C3D_RenderTarget* dst, bool toScreen, int quadW, int quadH, float du, float dv, float insetTexels) {
    const float umax = (float)s_w / (float)s_texW;
    const float vmin = 1.0f - (float)s_h / (float)s_texH;
    const float insetU = insetTexels / (float)s_texW;
    const float insetV = insetTexels / (float)s_texH;
    const float umaxEff = umax - insetU > 0.0f ? umax - insetU : 0.0f;
    const float vminEff = vmin + insetV < 1.0f ? vmin + insetV : 1.0f;

    const float xs[4] = {0.0f, (float)quadW, (float)quadW, 0.0f};
    const float ys[4] = {0.0f, 0.0f, (float)quadH, (float)quadH};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) {
        const int k = idx[i];
        const float u = xs[k] / (float)quadW * umaxEff;
        const float v = 1.0f - ys[k] / (float)quadH * (1.0f - vminEff);
        Vert& o = s_v[i];
        o.x = xs[k];
        o.y = ys[k];
        o.u0 = u;
        o.v0 = v;
        // The left/top taps may cross the texture edge; CLAMP_TO_EDGE there
        // repeats the scene's own edge, not the cleared unused area.
        o.u1 = u - du;
        o.v1 = v - dv;
        o.u2 = u + du;
        o.v2 = v + dv;
        o.r = o.g = o.b = o.a = 1.0f;
    }
    GSPGPU_FlushDataCache(s_v, sizeof(Vert) * 6);

    C3D_FrameDrawOn(dst);
    if (!toScreen) C3D_SetViewport(0, (u32)(s_texH - s_h), (u32)s_w, (u32)s_h);

    C3D_BindProgram(&s_prog);
    C3D_AttrInfo* ai = C3D_GetAttrInfo();
    AttrInfo_Init(ai);
    AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 2);   // position
    AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 2);   // uv0
    AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);   // uv1
    AttrInfo_AddLoader(ai, 3, GPU_FLOAT, 2);   // uv2
    AttrInfo_AddLoader(ai, 4, GPU_FLOAT, 4);   // colour
    C3D_BufInfo* bi = C3D_GetBufInfo();
    BufInfo_Init(bi);
    BufInfo_Add(bi, s_v, sizeof(Vert), 5, 0x43210);

    C3D_Mtx proj;
    if (toScreen) {
        // Same projection the UI uses on the top screen: y = 0 is the top row.
        Mtx_OrthoTilt(&proj, 0.0f, (float)quadW, (float)quadH, 0.0f, 1.0f, -1.0f, true);
    } else {
        Mtx_Ortho(&proj, 0.0f, (float)quadW, (float)quadH, 0.0f, 1.0f, -1.0f, false);
    }
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uProj, &proj);

    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
}

}   // namespace

bool init() {
    if (s_dvlb) return true;
    s_dvlb = DVLB_ParseFile((u32*)fsr_blit_shbin, fsr_blit_shbin_size);
    if (!s_dvlb) return false;
    shaderProgramInit(&s_prog);
    shaderProgramSetVsh(&s_prog, &s_dvlb->DVLE[0]);
    s_uProj = shaderInstanceGetUniformLocation(s_prog.vertexShader, "projection");
    s_v = (Vert*)linearAlloc(sizeof(Vert) * 6);
    if (s_uProj < 0 || !s_v) {
        if (s_v) { linearFree(s_v); s_v = nullptr; }
        shaderProgramFree(&s_prog);
        DVLB_Free(s_dvlb);
        s_dvlb = nullptr;
        return false;
    }
    // A tiny texture for units 1/2 after the pass: the scene texture may be
    // deleted on a mode change, and a bound (freed) texture is never safe.
    s_dummyOk = C3D_TexInit(&s_dummy, 8, 8, GPU_RGBA8);
    return true;
}

void shutdown() {
    dropTarget();
    if (s_dummyOk) { C3D_TexDelete(&s_dummy); s_dummyOk = false; }
    if (s_v) { linearFree(s_v); s_v = nullptr; }
    if (s_dvlb) {
        shaderProgramFree(&s_prog);
        DVLB_Free(s_dvlb);
        s_dvlb = nullptr;
    }
}

void release() { dropTarget(); }

bool ready() { return s_target != nullptr; }
bool tempReady() { return s_targetB != nullptr; }

int width() { return s_w; }
int height() { return s_h; }
int texWidth() { return s_texW; }
int texHeight() { return s_texH; }
C3D_RenderTarget* target() { return s_target; }

int vramBlocks(int w, int h, bool needTemp, u32 blocks[3]) {
    const u32 tw = (u32)pot(w), th = (u32)pot(h);
    const bool needA = !s_target || tw > (u32)s_texW || th > (u32)s_texH;
    const u32 btw = needA ? tw : (u32)s_texW;   // the intermediate matches the scene texture
    const u32 bth = needA ? th : (u32)s_texH;
    int n = 0;
    if (needA) {
        blocks[n++] = tw * th * 4u;   // RGBA8 colour texture
        blocks[n++] = tw * th * 3u;   // DEPTH24
    }
    if (needTemp && (!s_targetB || needA)) blocks[n++] = btw * bth * 4u;   // CAS intermediate
    return n;
}

bool ensure(int w, int h, bool needTemp) {
    if (!s_v) return false;
    if (w < 8) w = 8;
    if (h < 8) h = 8;
    const int tw = pot(w), th = pot(h);

    if (!(s_target && tw <= s_texW && th <= s_texH)) {
        dropTarget();

        if (!vramRoomFor((u32)tw * (u32)th * 4u)) return false;
        if (!C3D_TexInitVRAM(&s_tex, (u16)tw, (u16)th, GPU_RGBA8)) return false;
        s_texOk = true;
        C3D_TexSetFilter(&s_tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&s_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

        if (!vramRoomFor((u32)tw * (u32)th * 3u)) {
            C3D_TexDelete(&s_tex);
            s_texOk = false;
            return false;
        }
        s_target = C3D_RenderTargetCreateFromTex(&s_tex, GPU_TEXFACE_2D, 0, C3D_DEPTHTYPE(GPU_RB_DEPTH24));
        if (!s_target) {
            C3D_TexDelete(&s_tex);
            s_texOk = false;
            return false;
        }
        s_texW = tw;
        s_texH = th;
    }
    s_w = w;
    s_h = h;

    if (!needTemp) {
        dropTemp();
        return true;
    }

    if (!s_targetB) {
        const u32 btw = (u32)s_texW, bth = (u32)s_texH;   // same POT size as the scene texture
        if (!vramRoomFor(btw * bth * 4u)) return true;   // scene is usable, CAS falls back
        if (!C3D_TexInitVRAM(&s_texB, (u16)btw, (u16)bth, GPU_RGBA8)) return true;
        s_texBOk = true;
        C3D_TexSetFilter(&s_texB, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&s_texB, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        s_targetB = C3D_RenderTargetCreateFromTex(&s_texB, GPU_TEXFACE_2D, 0, C3D_DEPTHTYPE(-1));
        if (!s_targetB) {
            C3D_TexDelete(&s_texB);
            s_texBOk = false;
        }
    }
    return true;
}

void beginPass() {
    if (!s_target) return;
    C3D_FrameDrawOn(s_target);
    // The scene occupies the top rows of the texture (PICA's viewport Y is
    // measured from the bottom); the passes sample v in [1-h/th, 1].
    C3D_SetViewport(0, (u32)(s_texH - s_h), (u32)s_w, (u32)s_h);
}

void upscale(C3D_RenderTarget* dst, int outW, int outH, Filter filter, int casStrength, int casGate) {
    if (!s_target || !s_v || !dst) return;

    const float du = 1.0f / (float)s_texW;   // one source texel
    const float dv = 1.0f / (float)s_texH;

    if (filter == Filter::Cas && s_targetB) {
        // Horizontal: scene -> intermediate, RGB = X, alpha = the CAS weight.
        fsr3dsCasSetGate(casGate);
        fsr3dsTevKernel(FSR3DS_KERNEL_CAS_H, casStrength);
        bindTex(&s_tex);
        drawQuad(s_targetB, false, s_w, s_h, du, 0.0f, 0.0f);

        // Vertical: intermediate -> screen, upscaled; reads the weight from alpha.
        fsr3dsTevKernel(FSR3DS_KERNEL_CAS_V, 0);
        bindTex(&s_texB);
        drawQuad(dst, true, outW, outH, 0.0f, dv, 2.0f);
    } else {
        // LITE (also the fallback when the CAS intermediate could not be allocated).
        fsr3dsTevKernel(FSR3DS_KERNEL_LITE, 0);
        bindTex(&s_tex);
        drawQuad(dst, true, outW, outH, du, dv, 2.0f);
    }

    if (s_dummyOk) {   // the scene/intermediate texture may be released later
        C3D_TexBind(1, &s_dummy);
        C3D_TexBind(2, &s_dummy);
    }

    // What the 3D passes and uiDraw expect afterwards.
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
}

}   // namespace fsrScene
