//
// Voxel tunnel, with depth fog
//
// A photographed height field wrapped around a cylinder and seen from
// inside it. Classic bitmap tunnels invert a constant radius in closed
// form, v ∝ 1/r. The radius here depends on depth, so that invert is the
// first root of an implicit and is marched. voxeltunnel.cpp is this
// march with nothing moving but the flight. Here the flight is the
// same and the tube twists as it passes.
//
// The lens is a pinhole on the tunnel axis, Z forward, X right, Y down
// with the framebuffer:
//
//   x = cx + fx · X / Z
//   y = cy + fy · Y / Z
//
// A pixel inverts to a ray whose cylindrical radius grows linearly,
//
//   X/Z   = (x − cx) / fx
//   Y/Z   = (y − cy) / fy
//   θ     = atan2(Y, X)
//   slope = sqrt((X/Z)² + (Y/Z)²)
//   ρ(Z)  = slope · Z
//
// fx is 180, so 2 atan((W/2)/fx) is about 83 degrees across. fy is a
// little shorter (fx / 1.08), so the view is taller than it is wide —
// the same choice the terrain lens makes with a shorter focaly, and not
// a pixel-aspect correction. The framebuffer's pixels are square.
//
// (cx, cy) is the principal point, and it sits at the centre of the
// framebuffer and stays there. The mouth holds still, so the turning of
// the wall around it is the whole of the movement.
//
// The wall is a cylinder of radius WALL, grown inward by the height
// field. A stored byte is not a world height, so the peak in the map is
// scaled to RELIEF and the inner envelope is CORE = WALL − RELIEF:
//
//   R(θ, Z) = WALL − h(θ, Z) · RELIEF
//
// A hit is the first root of
//
//   f(Z) = slope · Z − R(θ, Z)
//
// f < 0 is still in the bore. The earliest a peak can be hit is
// Z = CORE / slope; the march begins there (or at the near plane) and
// steps with
//
//   dZ = STEP + LOD · Z
//
// so far samples thin out, the same coarsening the voxel columns use.
// A sign change is bisected BISECTIONS times; the high end of the last
// interval is the first sample with f ≥ 0. Rays that miss are the black
// aperture. The optical axis is a singularity of atan2 and of 1/slope,
// and is drawn as that aperture too.
//
// The map is a torus in (θ, Z). Around the tube and along it:
//
//   u = (θ + twist(Z, t)) · N / 2π
//   v = FLIGHT · t + DEPTH · Z
//
// twist is a roll and a helix, and both swing rather than run,
//
//   twist = ease(t) · [ 0.9 sin(0.31 t) + 0.005 sin(0.9 t) · Z ]
//
// so the tube winds up, unwinds through straight and winds the other
// way. A sine crosses zero at its fastest, so both terms would be at
// full speed on the first frame; ease is a smoothstep over the opening
// seconds, and the tube gathers its swing instead of arriving mid-one.
// The helix is the term that reads as a twist rather than as a
// spin: a roll turns every depth by the same angle at once, which is
// the picture rotating, while radians per world Z turn each depth by a
// different angle, which is a spiral standing in the tube, and flying
// into it drags the ground around the mouth. The two speeds are
// incommensurate, so the wind and the roll do not come back together
// on a beat. N is a power of two, so a sample wraps by a floor and a
// mask: a cast toward zero would fold (−1, 0) onto texel 0, and θ
// lives in [−π, π].
//
// Fog is linear in Z: each source colour c at shade k is the nearest
// palette entry to
//
//   (k / (SHADES − 1)) · RGB(c)
//
// A photograph's palette has nowhere clean to go on the way down — an
// entry with no dark relatives is matched to whichever few dark colours
// the picture happened to hold, so the far wall turns to mud short of
// the hole rather than reaching it. The free entries above the picture
// carry a ramp from black to the average of the picture, which is the
// road a darkening colour travels; the aperture is the black end of it.
// The shade of a hit is
//
//   k = floor( (Z_far − Z) / (Z_far − Z_near) · SHADES )
//
// clamped into [0, SHADES). The fade spans the whole march rather than
// the depths most walls stand at: the near field is barely touched and
// only the deepest wall goes dark, which is what leaves the tunnel its
// length. That is 1/Z fog's cheaper cousin: equal steps of Z, not of
// screen radius.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroshadetable.h"

#define TEXTURE_SIZE 256
#define TEXTURE_MASK (TEXTURE_SIZE - 1)
#define ANGLE_TO_U (TEXTURE_SIZE / (2.0 * M_PI)) // texels of u per radian of θ
#define TUNNEL_PICTURE_COLORS 221 // entries the photograph uses; the rest are free
#define TUNNEL_FADE_RAMP 224 // first free entry the fade to black takes
#define TUNNEL_APERTURE TUNNEL_FADE_RAMP // the black the darkest shade reaches

#define TUNNEL_WALL 126.0 // cylinder radius, world units
#define TUNNEL_RELIEF 76.0 // inward height of a peak, world units
#define TUNNEL_CORE (TUNNEL_WALL - TUNNEL_RELIEF)

#define TUNNEL_FOCAL_X 180.0 // pinhole focal, x pixels
#define TUNNEL_FOCAL_Y (TUNNEL_FOCAL_X / 1.08) // shorter, so the view is taller
#define TUNNEL_NEAR 18.0 // nearest depth the march will sample
#define TUNNEL_FAR 1120.0 // furthest depth; beyond this is the aperture
#define TUNNEL_STEP 4.0 // base world units between samples
#define TUNNEL_LOD 0.004 // added fraction of Z, so far samples thin out
#define TUNNEL_BISECTIONS 6 // halvings of a sign-change interval
#define TUNNEL_AXIS_EPS (0.5 / TUNNEL_FOCAL_X) // half a pixel, in slope units
#define TUNNEL_FOG_SHADES 32 // steps from full colour to black
#define TUNNEL_FOG_RANGE (TUNNEL_FAR - TUNNEL_NEAR)

#define TUNNEL_FLIGHT 70.0 // texels of v per second
#define TUNNEL_DEPTH_TEXELS 0.2875 // texels of v per world Z

#define TUNNEL_ROLL 0.9 // radians the map rolls at the end of a swing
#define TUNNEL_ROLL_SPEED 0.31 // radians a second that swing turns
#define TUNNEL_TWIST_PER_Z 0.005 // helix at full wind, radians per world Z
#define TUNNEL_TWIST_SPEED 0.9 // radians a second the wind turns
#define TUNNEL_TWIST_EASE 8.0 // seconds the swing takes to reach full

static RETRO_Image *Terrain;
static const unsigned char *HeightPixels;
static double HeightScale;
static double TwistBase;
static double TwistPerZ;
static double ScrollV;
static unsigned char FogTable[RETRO_COLORS * TUNNEL_FOG_SHADES];

// N is a power of two, so the torus wrap is a floor and a mask. A cast
// toward zero folds the cells either side of 0 onto the same texel.
static int WrapTexel(double t)
{
	return ((int)floor(t)) & TEXTURE_MASK;
}

static void MapTexel(double theta, double z, int *tx, int *ty)
{
	double twist = TwistBase + z * TwistPerZ;
	*tx = WrapTexel((theta + twist) * ANGLE_TO_U);
	*ty = WrapTexel(ScrollV + z * TUNNEL_DEPTH_TEXELS);
}

// R(θ, Z) = WALL − h(θ, Z) · RELIEF. Terrain grows inward from the
// cylinder, so high ground is encountered earlier and can hide the
// ground behind it.
static double WallRadius(double theta, double z)
{
	int tx;
	int ty;
	MapTexel(theta, z, &tx, &ty);
	return TUNNEL_WALL - HeightPixels[ty * TEXTURE_SIZE + tx] * HeightScale;
}

// f(Z) = slope · Z − R(θ, Z). Negative is still in the bore.
static double RadialImplicit(double slope, double theta, double z)
{
	return slope * z - WallRadius(theta, z);
}

// First root of f, or −1 if the ray misses. A sign change is bisected;
// the high end is the first sample with f ≥ 0.
static double IntersectWall(double slope, double theta)
{
	double z0 = MAX(TUNNEL_NEAR, TUNNEL_CORE / slope);
	double f0 = RadialImplicit(slope, theta, z0);
	if (f0 >= 0.0) {
		return z0;
	}

	double previousz = z0;
	double previous = f0;
	for (double z = z0 + TUNNEL_STEP; z <= TUNNEL_FAR; ) {
		double f = RadialImplicit(slope, theta, z);
		if (previous < 0.0 && f >= 0.0) {
			double low = previousz;
			double high = z;
			for (int i = 0; i < TUNNEL_BISECTIONS; i++) {
				double middle = (low + high) * 0.5;
				if (RadialImplicit(slope, theta, middle) >= 0.0) {
					high = middle;
				} else {
					low = middle;
				}
			}
			return high;
		}
		previous = f;
		previousz = z;
		z += TUNNEL_STEP + z * TUNNEL_LOD;
	}
	return -1.0;
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	const unsigned char *texture = Terrain->data;

	// A sine swings hardest as it crosses zero, so a swing at full
	// amplitude is at its fastest on the first frame. The ease holds both
	// terms near nothing while it is small, and the tube starts from rest.
	double ease = MIN(1.0, time / TUNNEL_TWIST_EASE);
	ease = ease * ease * (3.0 - 2.0 * ease);
	TwistBase = sin(time * TUNNEL_ROLL_SPEED) * TUNNEL_ROLL * ease;
	TwistPerZ = sin(time * TUNNEL_TWIST_SPEED) * TUNNEL_TWIST_PER_Z * ease;
	ScrollV = time * TUNNEL_FLIGHT;

	double cx = RETRO_WIDTH * 0.5;
	double cy = RETRO_HEIGHT * 0.5;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double rx = (x - cx) / TUNNEL_FOCAL_X;
			double ry = (y - cy) / TUNNEL_FOCAL_Y;
			double slope = hypot(rx, ry);
			if (slope < TUNNEL_AXIS_EPS) {
				buffer[y * RETRO_WIDTH + x] = TUNNEL_APERTURE;
				continue;
			}

			double theta = atan2(ry, rx);
			double hitz = IntersectWall(slope, theta);
			if (hitz < 0.0) {
				buffer[y * RETRO_WIDTH + x] = TUNNEL_APERTURE;
				continue;
			}

			int tx;
			int ty;
			MapTexel(theta, hitz, &tx, &ty);
			int shade = CLAMP((TUNNEL_FAR - hitz) / TUNNEL_FOG_RANGE
				* TUNNEL_FOG_SHADES, 0, TUNNEL_FOG_SHADES);
			buffer[y * RETRO_WIDTH + x] =
				FogTable[texture[ty * TEXTURE_SIZE + tx] * TUNNEL_FOG_SHADES + shade];
		}
	}
}

void DEMO_Initialize(void)
{
	Terrain = RETRO_LoadImage("assets/voxel_color_256x256.pcx", true);
	RETRO_Image *heightmap = RETRO_LoadImage("assets/voxel_height_256x256.pcx");
	if (Terrain->width != TEXTURE_SIZE || Terrain->height != TEXTURE_SIZE
		|| heightmap->width != TEXTURE_SIZE || heightmap->height != TEXTURE_SIZE) {
		RETRO_RageQuit("Terrain color and height maps must be 256x256\n");
	}
	HeightPixels = heightmap->data;

	int maxheight = 1;
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		if (HeightPixels[i] > maxheight) {
			maxheight = HeightPixels[i];
		}
	}
	// Scale from the peak that is actually present so the tallest point
	// reaches the inner wall even if a source map does not fill the range.
	HeightScale = TUNNEL_RELIEF / maxheight;

	// The photograph owns its palette and holds no fade of its own: scaled
	// toward black its colours land on the few dark browns it happens to
	// contain, and the far wall turns to mud short of the hole. It stops
	// short of the top of the range, so the free entries above it take a
	// ramp from black to the average of the picture, and a darkening colour
	// has clean ground to cross. The aperture is the black end of that
	// ramp, which is where the darkest shade of every colour lands too.
	RETRO_Palette palette[RETRO_COLORS];
	memcpy(palette, Terrain->palette, sizeof(palette));

	int r = 0;
	int g = 0;
	int b = 0;
	for (int i = 0; i < TUNNEL_PICTURE_COLORS; i++) {
		r += palette[i].r;
		g += palette[i].g;
		b += palette[i].b;
	}
	r /= TUNNEL_PICTURE_COLORS;
	g /= TUNNEL_PICTURE_COLORS;
	b /= TUNNEL_PICTURE_COLORS;

	for (int i = 0; i < TUNNEL_FOG_SHADES; i++) {
		double level = (double)i / (TUNNEL_FOG_SHADES - 1);
		palette[TUNNEL_FADE_RAMP + i] = RETRO_Palette{
			(unsigned char)(r * level),
			(unsigned char)(g * level),
			(unsigned char)(b * level),
		};
		RETRO_SetColor(TUNNEL_FADE_RAMP + i, palette[TUNNEL_FADE_RAMP + i].r,
			palette[TUNNEL_FADE_RAMP + i].g, palette[TUNNEL_FADE_RAMP + i].b);
	}

	RETRO_CreatePaletteShadeTable(palette, RETRO_COLORS, TUNNEL_FOG_SHADES, FogTable);
}
