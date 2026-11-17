/*
 * libFSR3DS v0.3 - PICA200 TEV post-processing kernels.
 */
#include "fsr3ds.h"

#include <math.h>

uint32_t fsr3dsColor(int r, int g, int b, int a)
{
	if (r < 0) r = 0;
	if (r > 255) r = 255;
	if (g < 0) g = 0;
	if (g > 255) g = 255;
	if (b < 0) b = 0;
	if (b > 255) b = 255;
	if (a < 0) a = 0;
	if (a > 255) a = 255;
	return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

void fsr3dsTevReset(void)
{
	for (int i = 0; i < 6; i++)
		C3D_TexEnvInit(C3D_GetTexEnv(i));
	C3D_TexEnvBufUpdate(C3D_Both, 0);
	C3D_TexEnvBufColor(0xFFFFFFFF);
}

static void kernel_replace(void)
{
	C3D_TexEnv *e = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e, C3D_Both, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e, C3D_Both, GPU_REPLACE);
}

void fsr3dsTevModulate(void)
{
	fsr3dsTevReset();
	C3D_TexEnv *e = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(e, C3D_Both, GPU_MODULATE);
}

/*
 * v0.1 3-sample kernel:
 *   B = (L+R)/2, D = C-B+0.5, out = C+D-0.5 = 2C-B
 */
static void kernel_lite(void)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_REPLACE);
}

/* Horizontal pass: RGB = 2C-B, Alpha = 4*P*(1-P) = 1 - 4*(C-B)^2 */
static void kernel_adapt_h(void)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_PREVIOUS, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvOpAlpha(e2, GPU_TEVOP_A_SRC_R, GPU_TEVOP_A_ONE_MINUS_SRC_R, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_MODULATE);
	C3D_TexEnvScale(e2, C3D_Alpha, GPU_TEVSCALE_4);
	C3D_TexEnvColor(e2, 0xFFFFFFFF);
}

/* Vertical pass: out = lerp(2C-B, C, weight in T0.alpha) */
static void kernel_adapt_v(void)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e3 = C3D_GetTexEnv(3);
	C3D_TexEnvSrc(e3, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_TEXTURE0);
	C3D_TexEnvOpRgb(e3, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvFunc(e3, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e3, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e3, C3D_Alpha, GPU_REPLACE);
	C3D_TexEnvColor(e3, 0xFFFFFFFF);
}

/*
 * Simple 3-tap blur along the axis selected by the UV offsets:
 *   B = (L+R)/2, blur = (C+B)/2, out = lerp(C, blur, strength)
 */
static void kernel_aa_blur(int strength255)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvColor(e1, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_CONSTANT);
	C3D_TexEnvColor(e2, fsr3dsColor(255, 255, 255, strength255));
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_REPLACE);
}

/* Edge-aware AA, horizontal: RGB = blur, Alpha = 1-4*(C-B)^2 */
static void kernel_aa_edge(void)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	/* B2 in RGB, edge weight in Alpha */
	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e2, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_PREVIOUS, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvOpAlpha(e2, GPU_TEVOP_A_SRC_R, GPU_TEVOP_A_ONE_MINUS_SRC_R, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_MODULATE);
	C3D_TexEnvScale(e2, C3D_Alpha, GPU_TEVSCALE_4);

	/* blur = (C + B2)/2, keep edge weight in Alpha */
	C3D_TexEnv *e3 = C3D_GetTexEnv(3);
	C3D_TexEnvSrc(e3, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvColor(e3, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e3, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e3, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e3, C3D_Alpha, GPU_REPLACE);
}

/* Second pass of the edge-aware AA: use weight from T0.alpha */
static void kernel_aa_apply(void)
{
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvColor(e1, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_TEXTURE0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e2, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e2, C3D_Alpha, GPU_REPLACE);
	C3D_TexEnvColor(e2, 0xFFFFFFFF);
}

/* Single-pass adaptive AA: blur * weight + C * (1-weight) */
static void kernel_aa_edge_apply(void)
{
	kernel_aa_edge();

	C3D_TexEnv *e4 = C3D_GetTexEnv(4);
	C3D_TexEnvSrc(e4, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_PREVIOUS);
	C3D_TexEnvOpRgb(e4, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvFunc(e4, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e4, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e4, C3D_Alpha, GPU_REPLACE);
	C3D_TexEnvColor(e4, 0xFFFFFFFF);
}

/* PICA-CAS gate curve: 0 = balanced, 1 = soft, 2 = sharp. */
static int s_casGate = 0;

void fsr3dsCasSetGate(int gate)
{
	s_casGate = (gate < 0) ? 0 : (gate > 2) ? 2 : gate;
}

/*
 * Stage 2 alpha gate for the current curve:
 *   balanced: 1 - 2*d^2 = 2*(P*(1-P) + 0.25)
 *   soft:     1 - 4*d^2 = 4*(P*(1-P) + 0.00)
 *   sharp:    1 - 1*d^2 = 1*(P*(1-P) + 0.75)
 * The RGB part of the same stage must already be configured.
 */
static void cas_gate_alpha(C3D_TexEnv *e)
{
	C3D_TexEnvSrc(e, C3D_Alpha, GPU_PREVIOUS, GPU_PREVIOUS, GPU_CONSTANT);
	C3D_TexEnvOpAlpha(e, GPU_TEVOP_A_SRC_R, GPU_TEVOP_A_ONE_MINUS_SRC_R, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(e, C3D_Alpha, GPU_MULTIPLY_ADD);

	switch (s_casGate)
	{
	case 1: /* soft: q = 1 - 4*d^2 */
		C3D_TexEnvScale(e, C3D_Alpha, GPU_TEVSCALE_4);
		C3D_TexEnvColor(e, fsr3dsColor(0, 0, 0, 0));
		break;
	case 2: /* sharp: q = 1 - d^2 */
		C3D_TexEnvScale(e, C3D_Alpha, GPU_TEVSCALE_1);
		C3D_TexEnvColor(e, fsr3dsColor(0, 0, 0, 191)); /* 0.75 */
		break;
	default: /* balanced: q = 1 - 2*d^2 */
		C3D_TexEnvScale(e, C3D_Alpha, GPU_TEVSCALE_2);
		C3D_TexEnvColor(e, fsr3dsColor(0, 0, 0, 64)); /* 0.25 */
		break;
	}
}

/*
 * PICA-CAS horizontal pass.
 *
 *   B  = (L + R) / 2
 *   d  = C - B
 *   q  = clamp(1 - 2*d^2) = clamp(2*(P*(1-P) + 0.25))
 *   X  = C + s*q*d
 *   A  = s*q
 *
 * RGB = X, alpha = A.  The gate uses the same quadratic form as the
 * existing adaptive kernels but with the gentler 1-2*d^2 curve:
 * MULTIPLY_ADD(P, 1-P, 0.25) followed by alpha scale 2.
 */
static void kernel_cas_h(int strength255)
{
	/* Stage 0: B = INTERPOLATE(T1, T2, 0.5). */
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	/* Stage 1: P = C - B + 0.5. */
	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	/* Stage 2: RGB E = 2C - B; alpha q = 1 - 2*d^2. */
	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD_SIGNED);
	cas_gate_alpha(e2);

	/* Stage 3: keep E in RGB, scale the gate by strength in alpha. */
	C3D_TexEnv *e3 = C3D_GetTexEnv(3);
	C3D_TexEnvSrc(e3, C3D_RGB, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e3, C3D_RGB, GPU_REPLACE);
	C3D_TexEnvSrc(e3, C3D_Alpha, GPU_PREVIOUS, GPU_CONSTANT, 0);
	C3D_TexEnvOpAlpha(e3, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(e3, C3D_Alpha, GPU_MODULATE);
	C3D_TexEnvColor(e3, fsr3dsColor(0, 0, 0, strength255));

	/* Stage 4: X = INTERPOLATE(E, C, A); preserve A for the vertical pass. */
	C3D_TexEnv *e4 = C3D_GetTexEnv(4);
	C3D_TexEnvSrc(e4, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_PREVIOUS);
	C3D_TexEnvOpRgb(e4, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvFunc(e4, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e4, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e4, C3D_Alpha, GPU_REPLACE);
}

/*
 * PICA-CAS vertical pass.
 *
 * The input texture already contains X (horizontal result) in RGB and the
 * horizontal coefficient A in alpha.  For one vertical axis:
 *
 *   Bv = (U + D) / 2
 *   dv = X - Bv
 *   qv = clamp(1 - 2*dv^2)
 *   Y  = X + qv*A*dv
 *
 * Stage 3 combines qv with A; stage 4 applies the combined weight.  Stage 5
 * is left free for fsr3dsTevFx().
 */
static void kernel_cas_v(void)
{
	/* Stage 0: Bv = INTERPOLATE(T1, T2, 0.5). */
	C3D_TexEnv *e0 = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(e0, C3D_RGB, GPU_TEXTURE1, GPU_TEXTURE2, GPU_CONSTANT);
	C3D_TexEnvColor(e0, fsr3dsColor(128, 128, 128, 128));
	C3D_TexEnvFunc(e0, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e0, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e0, C3D_Alpha, GPU_REPLACE);

	/* Stage 1: Pv = X - Bv + 0.5. */
	C3D_TexEnv *e1 = C3D_GetTexEnv(1);
	C3D_TexEnvSrc(e1, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, 0);
	C3D_TexEnvOpRgb(e1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e1, C3D_RGB, GPU_ADD_SIGNED);
	C3D_TexEnvSrc(e1, C3D_Alpha, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e1, C3D_Alpha, GPU_REPLACE);

	/* Stage 2: RGB Ev = 2X - Bv; alpha qv = 1 - 2*dv^2. */
	C3D_TexEnv *e2 = C3D_GetTexEnv(2);
	C3D_TexEnvSrc(e2, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvOpRgb(e2, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(e2, C3D_RGB, GPU_ADD_SIGNED);
	cas_gate_alpha(e2);

	/* Stage 3: keep Ev in RGB; alpha = qv * A from the horizontal pass. */
	C3D_TexEnv *e3 = C3D_GetTexEnv(3);
	C3D_TexEnvSrc(e3, C3D_RGB, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(e3, C3D_RGB, GPU_REPLACE);
	C3D_TexEnvSrc(e3, C3D_Alpha, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvOpAlpha(e3, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(e3, C3D_Alpha, GPU_MODULATE);

	/* Stage 4: Y = INTERPOLATE(Ev, X, qv*A); final alpha is opaque. */
	C3D_TexEnv *e4 = C3D_GetTexEnv(4);
	C3D_TexEnvSrc(e4, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE0, GPU_PREVIOUS);
	C3D_TexEnvOpRgb(e4, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvFunc(e4, C3D_RGB, GPU_INTERPOLATE);
	C3D_TexEnvSrc(e4, C3D_Alpha, GPU_CONSTANT, 0, 0);
	C3D_TexEnvFunc(e4, C3D_Alpha, GPU_REPLACE);
	C3D_TexEnvColor(e4, 0xFFFFFFFF);
}

void fsr3dsTevKernel(Fsr3dsKernel kernel, int strength255)
{
	fsr3dsTevReset();

	switch (kernel)
	{
	case FSR3DS_KERNEL_REPLACE:
		kernel_replace();
		break;
	case FSR3DS_KERNEL_LITE:
		kernel_lite();
		break;
	case FSR3DS_KERNEL_ADAPT_H:
		kernel_adapt_h();
		break;
	case FSR3DS_KERNEL_ADAPT_V:
		kernel_adapt_v();
		break;
	case FSR3DS_KERNEL_AA_BLUR:
		kernel_aa_blur(strength255);
		break;
	case FSR3DS_KERNEL_AA_EDGE:
		kernel_aa_edge();
		break;
	case FSR3DS_KERNEL_AA_APPLY:
		kernel_aa_apply();
		break;
	case FSR3DS_KERNEL_AA_EDGE_APPLY:
		kernel_aa_edge_apply();
		break;
	case FSR3DS_KERNEL_CAS_H:
		kernel_cas_h(strength255);
		break;
	case FSR3DS_KERNEL_CAS_V:
		kernel_cas_v();
		break;
	default:
		kernel_replace();
		break;
	}
}

void fsr3dsTevFx(void)
{
	/* Stage 5 is free in every kernel used by the final pass. */
	C3D_TexEnv *e = C3D_GetTexEnv(5);
	C3D_TexEnvSrc(e, C3D_Both, GPU_PREVIOUS, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(e, C3D_Both, GPU_MODULATE);
}

/* ------------------------------------------------------------------------- */
/* Dynamic resolution                                                        */
/* ------------------------------------------------------------------------- */

static double s_drsHold;
static int s_drsDir;

int fsr3dsDrsUpdate(double frameMs, int *resIndex, int resCount, int targetFps, double dt)
{
	if (targetFps <= 0 || !resIndex || resCount <= 1)
		return 0;

	const double target = 1000.0 / (double)targetFps;
	s_drsHold -= dt;

	if (s_drsHold > 0.0)
		return 0;

	if (frameMs > target * 1.10 && *resIndex > 0)
	{
		(*resIndex)--;
		s_drsHold = 0.5;
		s_drsDir = -1;
		return -1;
	}

	if (frameMs < target * 0.90 && *resIndex < resCount - 1)
	{
		(*resIndex)++;
		s_drsHold = 1.0;
		s_drsDir = 1;
		return 1;
	}

	s_drsDir = 0;
	return 0;
}

const char *fsr3dsVersion(void)
{
	return FSR3DS_VERSION;
}
