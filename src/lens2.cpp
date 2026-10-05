//
// Lens, refracting glass
//
// Refraction: a magnifying lens as a real piece of glass, bending the
// picture by Snell's law and lit like one. The lens.cpp magnifier traced
// through physically, with reflection, a highlight, a tint and a shadow.
//
// A glass dome sliding over a picture, like a paperweight: a hemisphere of
// radius R whose flat side lies on the picture. The view is straight down
// z, orthographic, with the picture in the plane z = 0. For a pixel at
// (x, y) from the center, inside R, the ray d = (0, 0, 1) meets the dome at
//
//   P = (x, y, −sqrt(R² − x² − y²)),   N = P / R
//
// and bends to T = refract(d, N, 1 / IOR). The glass touches the picture,
// so there is no second surface to cross and the ray runs straight on to
//
//   sample = P + T (−P_z / T_z)
//
// wrapped, so a ray bent past the edge reads the picture's other side. One
// curved surface over a flat base magnifies gently, about IOR times at the
// center, and spreads a texel over more than a pixel, so the sample is
// bilinear between the four texels around it.
//
// The surface also reflects. Schlick's approximation gives the fraction
//
//   F = F0 + (1 − F0)(1 − cos θ)⁵,   F0 = ((IOR − 1) / (IOR + 1))²
//
// with cos θ = −d · N, so the glass is nearly clear head-on and turns into a
// mirror at the rim. The mirror shows a sky, SKY_LOW below the horizon to
// SKY_HIGH overhead, and a Phong highlight of the light. The refracted color
// is tinted by GLASS_TINT. Outside the dome the picture is seen directly,
// darkened by a soft shadow SHADOW_OFFSET away from the dome.
//
// The picture's palette stays, and every mixed color is mapped back to it
// through a 32³ inverse color LUT.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define DOME_RADIUS 56.0f // pixels
#define DOME_MARGIN 4 // kept between the dome and the screen edge
#define DOME_PERIOD 16.0 // seconds for the dome's path to close
#define IOR 1.5f // glass
#define SHININESS 80.0f
#define SHADOW_OFFSET 18.0f // pixels right and down
#define SHADOW_DEPTH 0.45f // fraction of the light the shadow takes away
#define SHADOW_SOFTNESS 14.0f // pixels over which the shadow's edge fades
#define GLASS_TINT vec3{ 0.88f, 0.97f, 1.0f }
#define SKY_LOW vec3{ 30, 30, 45 }
#define SKY_HIGH vec3{ 215, 225, 255 }

static RETRO_Image *Picture;
static unsigned char ColorLUT[32][32][32];

//
// The picture's color at a texel, wrapped onto it
//
static vec3 Texel(int x, int y)
{
	RETRO_Palette color = Picture->palette[Picture->data[WRAP(y, RETRO_HEIGHT) * RETRO_WIDTH + WRAP(x, RETRO_WIDTH)]];
	return { (float)color.r, (float)color.g, (float)color.b };
}

//
// The picture's color at a point, bilinear between the texel centers around it
//
static vec3 Sample(float x, float y)
{
	float fx = x - 0.5f;
	float fy = y - 0.5f;
	int ix = floor(fx);
	int iy = floor(fy);
	float u = fx - ix;
	float v = fy - iy;

	vec3 top = mix(Texel(ix, iy), Texel(ix + 1, iy), u);
	vec3 bottom = mix(Texel(ix, iy + 1), Texel(ix + 1, iy + 1), u);
	return mix(top, bottom, v);
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase. The dome rides a 2:3 Lissajous figure
	double phase = fract(time / DOME_PERIOD) * 2 * M_PI;
	float swingx = RETRO_WIDTH / 2.0f - DOME_RADIUS - DOME_MARGIN;
	float swingy = RETRO_HEIGHT / 2.0f - DOME_RADIUS - DOME_MARGIN;
	float cx = RETRO_WIDTH / 2.0f + swingx * sin(2 * phase + M_PI / 4);
	float cy = RETRO_HEIGHT / 2.0f + swingy * sin(3 * phase);

	float r = DOME_RADIUS;
	float f0 = (IOR - 1) * (IOR - 1) / ((IOR + 1) * (IOR + 1));
	vec3 d = { 0, 0, 1 };
	vec3 light = normalize(vec3{ -0.5f, -0.6f, -1.0f });
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float px = x + 0.5f - cx;
			float py = y + 0.5f - cy;
			float rr = px * px + py * py;
			vec3 color;

			if (rr < r * r) {
				// Through the glass
				vec3 p = { px, py, -sqrtf(r * r - rr) };
				vec3 n = p / r;
				vec3 t = refract(d, n, 1 / IOR);

				float reach = -p.z / t.z;
				vec3 refracted = Sample(cx + p.x + t.x * reach, cy + p.y + t.y * reach) * GLASS_TINT;

				// Off the glass
				vec3 bounce = reflect(d, n);
				vec3 sky = mix(SKY_LOW, SKY_HIGH, 0.5f - 0.5f * bounce.y);
				float highlight = 255 * powf(MAX(0.0f, dot(bounce, light)), SHININESS);
				float fresnel = mix(f0, 1, powf(1 + dot(d, n), 5));

				color = mix(refracted, sky, fresnel) + vec3{ highlight, highlight, highlight };
			} else {
				// The picture, in the dome's shadow
				float sx = px - SHADOW_OFFSET;
				float sy = py - SHADOW_OFFSET;
				float edge = CLAMP01((r - length(vec2{ sx, sy })) / SHADOW_SOFTNESS);
				color = Texel(x, y) * (1 - SHADOW_DEPTH * edge);
			}

			int red = CLAMP256(color.x);
			int green = CLAMP256(color.y);
			int blue = CLAMP256(color.z);
			buffer[y * RETRO_WIDTH + x] = ColorLUT[red >> 3][green >> 3][blue >> 3];
		}
	}
}

void DEMO_Initialize(void)
{
	Picture = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);

	// Map every shaded, tinted or reflected color back to the picture's palette
	RETRO_CreateColorLUT(Picture->palette, 32, &ColorLUT[0][0][0]);
}
