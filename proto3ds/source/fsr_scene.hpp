// fsr_scene.hpp — FSR-lite / PICA-CAS for w3dproto.
//
// The 3D scene is drawn into a smaller VRAM texture and post-processed by the
// PICA TEV (libFSR3DS, source/fsr3ds.c):
//   - LITE: one pass, 3-tap unsharp (2C - (L+R)/2);
//   - PICA-CAS: two passes (horizontal then vertical), contrast-adaptive
//     sharpening with a 0..255 strength and a gate curve.
// This is the FurMark research code, not AMD FidelityFX FSR/CAS: the PICA200
// has no fragment or compute shaders.
//
// The UI is drawn after the post-processing at the output resolution, so text
// and the HUD stay sharp. The module owns the scene colour texture, its depth
// buffer and (only for CAS) one intermediate colour texture.
#pragma once
#include <3ds.h>
#include <citro3d.h>

namespace fsrScene {

enum class Filter {
    Lite,   // one TEV pass (2C - (L+R)/2)
    Cas,    // PICA-CAS: H -> intermediate, V -> screen
};

// Loads the blit vertex shader. Call once after C3D_Init. No VRAM is taken.
bool init();
void shutdown();

// Makes sure a scene target with room for at least w x h pixels exists.
// needTemp also prepares the intermediate target for CAS. A missing CAS target
// does not make ensure() fail: target() stays usable and tempReady() is false.
bool ensure(int w, int h, bool needTemp);
void release();
bool ready();
bool tempReady();

// VRAM blocks C3D will allocate for ensure(w, h, needTemp): scene colour,
// scene depth, CAS intermediate. Filled only for the blocks that are missing.
// Returns the number of blocks (0..3).
int vramBlocks(int w, int h, bool needTemp, u32 blocks[3]);

int width();        // requested scene width (the viewport)
int height();       // requested scene height
int texWidth();     // power-of-two texture size
int texHeight();
C3D_RenderTarget* target();

// Binds the scene target and sets the viewport to the requested w x h part.
void beginPass();

// Post-processes the scene texture onto dst (a screen target), scaled to
// outW x outH. casStrength is 0..255 (PICA-CAS only). casGate: 0 balanced,
// 1 soft, 2 sharp. If CAS is requested but the intermediate target is missing,
// the LITE path is used instead.
void upscale(C3D_RenderTarget* dst, int outW, int outH, Filter filter, int casStrength, int casGate);

}   // namespace fsrScene
