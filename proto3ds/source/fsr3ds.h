/*
 * libFSR3DS v0.3 - public post-processing API.
 *
 * The library only depends on Citro3D/PICA200: no geometry, textures, input
 * handling or UI of the FurMark application.  The application owns the scene
 * targets and passes the source texture/dimensions to these helpers.
 *
 * All functions configure the fixed-function TEV pipeline; there are no
 * fragment/compute shaders on the PICA200.
 */
#pragma once

#include <3ds.h>
#include <citro3d.h>

#define FSR3DS_VERSION "0.3"

typedef enum
{
	FSR3DS_KERNEL_REPLACE = 0,   /* pass-through */
	FSR3DS_KERNEL_LITE,          /* v0.1 3-tap unsharp (H or V by UV offset) */
	FSR3DS_KERNEL_ADAPT_H,       /* 2C-B + edge weight in alpha (H pass) */
	FSR3DS_KERNEL_ADAPT_V,       /* weighted lerp(2C-B, C, weight) */
	FSR3DS_KERNEL_AA_BLUR,       /* 3-tap blur with constant strength */
	FSR3DS_KERNEL_AA_EDGE,       /* blur + edge weight in alpha (H pass) */
	FSR3DS_KERNEL_AA_APPLY,      /* weighted lerp(blur, C, weight) */
	FSR3DS_KERNEL_AA_EDGE_APPLY, /* single-pass adaptive blur (5 stages) */
} Fsr3dsKernel;

/* TEV constant colour register order: R in bits 0-7. */
uint32_t fsr3dsColor(int r, int g, int b, int a);

/* Reset all six TEV stages to REPLACE(PREVIOUS), scale 1, no buffer update. */
void fsr3dsTevReset(void);

/* Scene/UI pass-through: stage 0 MODULATE(texture0, vertex colour). */
void fsr3dsTevModulate(void);

/* Configure a complete kernel. strength255 is used by the AA kernels
 * (0 = keep centre, 255 = full blur). */
void fsr3dsTevKernel(Fsr3dsKernel kernel, int strength255);

/* Append the effects stage (stage 4): previous * vertex colour.
 * Vertex colours carry the vignette and colour grade. */
void fsr3dsTevFx(void);

/* Dynamic-resolution controller.  frameMs is a smoothed frame time.
 * Returns -1/0/+1 resolution-index delta and updates *resIndex. */
int fsr3dsDrsUpdate(double frameMs, int *resIndex, int resCount, int targetFps, double dt);

const char *fsr3dsVersion(void);
