//
// Flubber
//
// A 32-span elliptic cylinder, one scanline at a time. The cross-section is
// one 2D ellipse, reused on every row:
//
//   P_j = (RX cos θ_j, RZ sin θ_j),   θ_j = 2π j / N
//
// Row i yaws that ellipse by a time-varying angle and shifts it in x, then
// each edge is a textured span with a z-buffer. Vertices are rounded once.
// An edge with x_left >= x_right is back-facing and is skipped. Spans are
// half-open and clipped to [0, WIDTH).
//
//   spin(time)     = SPIN_SPEED · time
//   twistcos(time) = cos(TWIST_OMEGA · time)
//   twistmod(time) = sin(TWIST_MOD_OMEGA · time)
//   swaycos(time)  = cos(SWAY_OMEGA · time)
//   scroll(time)   = SCROLL_SPEED · time
//
//   angle(i) = spin + 3π (twistcos · twistmod · cos(π i / 3H) + 1)
//   xoffset(i) = CX + SWAY · swaycos · sin(π i / H)
//
// Rotation is x' = Px c − Pz s, z' = Px s + Pz c. Smaller z' is nearer.
// Texture u steps 16 texels per span (two wraps of 256). v is
// (y + scroll) mod 256.
//
// Lighting is a tent of the view-space tangent x — the normalized chord
// P_{j+1} − P_{j−1}, rotated with the ellipse:
//
//   idx    = (256 − 128 t_x) mod 256
//   lumel  = tent(idx)
//   color  = FlubberShadeTable[texel][lumel]
//
// tent(i) = 255 − |255 − 2i|, peaking at i = 127 and 128. The LUT is 240
// diffuse ramps plus 16 specular highlights, 240 + 16 = 256, independent of
// the screen height. Diffuse is C · (120 + s) / 360, from one-third ambient
// up to almost the face colour. Specular lerps toward white as
// C + (255 − C) · h / 23. lumel 240..255 is the tent peak, so the highlight
// sits on the silhouette rims.
//
// spin, scroll, twistcos, twistmod and swaycos live on the displayed clock.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"

#define SPANS 32
#define SPANMASK (SPANS - 1)
#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define DIFFUSE_SHADES 240
#define SPECULAR_SHADES 16
#define SHADE_LUT_WIDTH (DIFFUSE_SHADES + SPECULAR_SHADES)

#define FLUBBER_RX 24.0f
#define FLUBBER_RZ 110.0f
#define FLUBBER_CX (RETRO_WIDTH / 2.0)
#define FLUBBER_SWAY 40.0f // pixels of lateral sway
#define FLUBBER_TWIST (3 * M_PI) // radians of the (modulated + 1) yaw
#define FLUBBER_U_PER_SPAN (TEXTURE_WIDTH * 2.0f / SPANS) // 16 texels, two wraps of 256
#define FLUBBER_LIGHT_CENTER 128.0f // tent peak in the 8-bit light table
#define FLUBBER_Z_FAR 1.0e30f

#define FLUBBER_SCROLL_SPEED (TEXTURE_HEIGHT / 6.0) // texels per second, one wrap every 6s
#define FLUBBER_SPIN_SPEED (2 * M_PI / 12.0) // rad/s, one turn every 12s
#define FLUBBER_TWIST_OMEGA (2 * M_PI / 20.0) // rad/s, 20s period
#define FLUBBER_TWIST_MOD_OMEGA (2 * M_PI / 17.0) // rad/s, 17s period
#define FLUBBER_SWAY_OMEGA (2 * M_PI / 18.0) // rad/s, 18s period

#define FLUBBER_DIFFUSE_BASE 120 // (BASE + s) / DIV: s = 0 is 1/3 ambient
#define FLUBBER_DIFFUSE_DIV 360
#define FLUBBER_SPECULAR_DIV 23 // h / DIV of the way from C to white

static_assert(SHADE_LUT_WIDTH == 256, "lumel is an 8-bit index into a 256-wide LUT row");

long TextureScroll;
unsigned char LightTable[RETRO_COLORS];
unsigned char FlubberShadeTable[RETRO_COLORS * SHADE_LUT_WIDTH];
float ZBuffer[RETRO_WIDTH * RETRO_HEIGHT];

vec3 EllipsePoints[SPANS];
vec3 EllipseTangents[SPANS];

//
// One scanline of one face, half-open in x. A back-facing edge has
// x1 >= x2 and covers nothing, which is the silhouette test.
//
// u, lighting and z are affine in x. Gradients are taken from the
// unclipped edge so a clip on the left still samples the edge at x = 0,
// not at the off-screen vertex. v is constant on the row: the texture
// crawls in y only.
//
// lumel indexes FlubberShadeTable[texel][0..DIFFUSE_SHADES) diffuse /
// [DIFFUSE_SHADES..SHADE_LUT_WIDTH) specular. Smaller z is nearer.
//
static void DrawSpan(int y, int x1, float u1, float l1, float z1, int x2, float u2, float l2, float z2)
{
	if (x1 >= x2) {
		return;
	}

	int dx = x2 - x1;
	float du = (u2 - u1) / dx;
	float dl = (l2 - l1) / dx;
	float dz = (z2 - z1) / dx;

	// Advance attributes by the pixels the left clip discards
	if (x1 < 0) {
		u1 += du * -x1;
		l1 += dl * -x1;
		z1 += dz * -x1;
		x1 = 0;
	}
	x2 = MIN(x2, RETRO_WIDTH);
	if (x1 >= x2) {
		return;
	}

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();
	int *yoffset = RETRO_Yoffset();

	long offs = yoffset[y] + x1;
	int v = WRAP(y + TextureScroll, TEXTURE_HEIGHT) * TEXTURE_WIDTH;

	for (int x = x1; x < x2; x++) {
		if (z1 < ZBuffer[offs]) {
			int texel = image[v + WRAP((int)u1, TEXTURE_WIDTH)];
			int lumel = LightTable[WRAP256((int)l1)];

			buffer[offs] = FlubberShadeTable[texel * SHADE_LUT_WIDTH + lumel];
			ZBuffer[offs] = z1;
		}

		l1 += dl;
		u1 += du;
		z1 += dz;

		offs++;
	}
}

void DEMO_Render(double time, double deltatime)
{
	float screenx[SPANS];
	float viewz[SPANS];
	float lighting[SPANS];

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		ZBuffer[i] = FLUBBER_Z_FAR;
	}

	TextureScroll = (long)(time * FLUBBER_SCROLL_SPEED);

	// Calculate phase. The three oscillators have incommensurate periods
	// (20s, 17s, 18s) so the pose does not obviously loop.
	float spin = (float)(time * FLUBBER_SPIN_SPEED);
	float twistcos = cos(time * FLUBBER_TWIST_OMEGA);
	float twistmod = sin(time * FLUBBER_TWIST_MOD_OMEGA);
	float swaycos = cos(time * FLUBBER_SWAY_OMEGA);

	for (int i = 0; i < RETRO_HEIGHT; i++) {
		// Half-sine bow down the column, scaled by the slow sway oscillator
		float xoffset = FLUBBER_CX + FLUBBER_SWAY * swaycos * sin(i * M_PI / RETRO_HEIGHT);
		// Always 3π of yaw, plus a twist that varies slowly down the height
		float angle = spin + FLUBBER_TWIST * (twistcos * cos(i * M_PI / (3 * RETRO_HEIGHT)) * twistmod + 1);
		float cosangle = cos(angle);
		float sinangle = sin(angle);

		for (int j = 0; j < SPANS; j++) {
			// Same 2D rotation as the point: x' = x c − z s, z' = x s + z c
			float tangentx = EllipseTangents[j].x * cosangle + EllipseTangents[j].z * -sinangle;
			screenx[j] = EllipsePoints[j].x * cosangle + EllipsePoints[j].z * -sinangle + xoffset;
			viewz[j] = EllipsePoints[j].x * sinangle + EllipsePoints[j].z * cosangle;
			// idx = (256 − 128 t_x) mod 256. Rim (±x) lands on the tent peak.
			lighting[j] = 2 * FLUBBER_LIGHT_CENTER - FLUBBER_LIGHT_CENTER * tangentx;
		}

		// u steps 16 texels per span, 32 × 16 = 512 = two wraps of the 256 texture
		for (int j = 0; j < SPANS; j++) {
			int jn = (j + 1) & SPANMASK;
			DrawSpan(i, (int)lround(screenx[j]), j * FLUBBER_U_PER_SPAN, lighting[j], viewz[j],
					 (int)lround(screenx[jn]), (j + 1) * FLUBBER_U_PER_SPAN,
					 lighting[jn], viewz[jn]);
		}
	}
}

// Nearest palette entry in RGB, by d² = Δr² + Δg² + Δb²
static unsigned char ClosestColor(RETRO_Palette *palette, unsigned char r, unsigned char g, unsigned char b)
{
	long dist = 1 << 30;
	unsigned char color = 0;

	for (int i = 0; i < RETRO_COLORS; i++) {
		long newdist = (r - palette[i].r) * (r - palette[i].r) + (g - palette[i].g) * (g - palette[i].g) + (b - palette[i].b) * (b - palette[i].b);
		if (newdist == 0) {
			return i;
		}
		if (newdist < dist) {
			color = i;
			dist = newdist;
		}
	}

	return color;
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);
	RETRO_Palette *palette = RETRO_ImagePalette();

	// Precalculate the light values. tent(i) = 255 − |255 − 2i|, 0 at both
	// ends, peak 254 at i = 127 and 128. Specular is lumel 240..255, the
	// ~16 samples around that peak, so the highlight sits on the rims.
	for (int i = 0; i < RETRO_COLORS; i++) {
		LightTable[i] = 255 - CLAMP256(abs(255 - i * 2));
	}

	// Calculate light reflections. Each palette colour is shaded against
	// itself and snapped back into the same palette. Diffuse is
	// C · (120 + s) / 360, from one-third ambient up to almost C.
	// Specular lerps toward white as C + (255 − C) · h / 23 and never
	// quite reaches it (h max is 15).
	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = palette[i].r;
		unsigned char g = palette[i].g;
		unsigned char b = palette[i].b;

		for (int s = 0; s < DIFFUSE_SHADES; s++) {
			FlubberShadeTable[i * SHADE_LUT_WIDTH + s] = ClosestColor(palette,
				r * (FLUBBER_DIFFUSE_BASE + s) / FLUBBER_DIFFUSE_DIV,
				g * (FLUBBER_DIFFUSE_BASE + s) / FLUBBER_DIFFUSE_DIV,
				b * (FLUBBER_DIFFUSE_BASE + s) / FLUBBER_DIFFUSE_DIV);
		}

		for (int h = 0; h < SPECULAR_SHADES; h++) {
			FlubberShadeTable[i * SHADE_LUT_WIDTH + h + DIFFUSE_SHADES] = ClosestColor(palette,
				r + (255 - r) * h / FLUBBER_SPECULAR_DIV,
				g + (255 - g) * h / FLUBBER_SPECULAR_DIV,
				b + (255 - b) * h / FLUBBER_SPECULAR_DIV);
		}
	}

	// Create flubber. One 2D ellipse in xz, reused on every scanline.
	// The "normal" used for lighting is the unit tangent from the
	// central difference P_{j+1} − P_{j−1}, not the radial normal.
	for (int j = 0; j < SPANS; j++) {
		float theta = j * (float)M_PI * 2 / SPANS;
		EllipsePoints[j].x = FLUBBER_RX * cos(theta);
		EllipsePoints[j].z = FLUBBER_RZ * sin(theta);
	}

	for (int j = 0; j < SPANS; j++) {
		vec3 chord = EllipsePoints[(j + 1) & SPANMASK] - EllipsePoints[(j - 1) & SPANMASK];
		EllipseTangents[j] = normalize(chord);
	}
}
