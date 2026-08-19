//
// Twister 3
//
// A square column drawn one scanline at a time, wrapped in the flowers
// picture, which climbs the two faces that are turned toward the viewer.
//
// The cross section is a square whose four corners ride a circle of radius
// R = RADIUS, so the column measures 2R across corner on and R sqrt 2 face
// on. With the viewer at +z, corner k of the cross section at phase position i
// sits at
//
//   a_k = i * TURNS * CYCLE + k * 64      (angle table units)
//   x_k = W/2 - R cos a_k
//   z_k = R sin a_k
//
// Only x is stored, because z decides the rest on its own. The leftmost
// corner is the one whose angle is nearest 0, so the corner a quarter turn
// on has the largest sine and is the one nearest the viewer, the corner
// half a turn on is the rightmost, and the fourth is hidden behind the
// other three. The two visible faces are therefore always
//
//   leftmost -> nearest -> rightmost
//
// and three x values per scanline describe the whole silhouette. No depth
// test or sort is needed, and no face is ever drawn over another.
//
// The square turns TURNS = 1.5 times over the period. Three half turns is six
// quarter turns, and a quarter turn maps the square onto itself, so the shape
// wraps without a seam.
//
// A scanline evaluates the shape at
//
//   index = y * torsion(phase) + phase
//
// so torsion is the table units of twist per scanline. It is a wave in the
// phase rather than in y, seven cycles over the period under an envelope
// that closes once,
//
//   torsion(i) = TORSION * sin(2 pi * WAVE * i / N) * cos(2 pi * i / N)
//
// which is what makes the column wind up tight, unwind to a straight prism
// where the wave crosses zero, and then corkscrew the other way.
//
// The surface is the flowers picture, TWISTER_IMAGE_SIZE square. It is wrapped once
// around the column rather than repeated on every face: a side carries
// TWISTER_IMAGE_FACE = TWISTER_IMAGE_SIZE / 4 texels, and which quarter of the picture a face
// carries is named by the corner it starts at,
//
//   u = corner * IMAGE_FACE .. + IMAGE_FACE   around the column
//   v = y + TWISTER_IMAGE_SCROLL * phase
//                                             along it, wrapped on TWISTER_IMAGE_SIZE
//
// so the picture runs on across the crease and turns with the column, rather
// than the same quarter appearing twice over on the two visible faces. That is
// what the leftmost corner's own number is kept for. A face carries its quarter
// however wide it is on screen, so the picture squeezes as the face turns
// away. That stretch is what reads as perspective, there being none in the
// projection. The twist travels too, since a row holds its place in the shape
// only where y * torsion + phase stays put, but torsion changes sign with its
// wave and that motion turns around with it. The scroll is the one thing on
// the column that only ever goes up.
//
// The picture is a photograph and uses all RETRO_COLORS entries of its own
// palette, so there is no ramp left over to shade a texel along and the column
// is drawn flat. Its form comes from the silhouette and the squeeze.
//
// phase lives on TWISTER_PERIOD and advances TWISTER_SPEED table units a
// second.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TWISTER_PERIOD 512 // one whole cycle of the column and four picture scrolls
#define TWISTER_CYCLE ((double)RETRO_SINCOS_ANGLE / TWISTER_PERIOD) // angle units a once-round wave advances per phase unit
#define TWISTER_CENTER_X (RETRO_WIDTH / 2)
#define TWISTER_RADIUS 60 // the circle the four corners ride, half the column's width corner on
#define TWISTER_TURNS 1.5 // turns of the square over the period, a whole number of quarter turns
#define TWISTER_TORSION 1.2 // table units the column twists per scanline where the torsion wave peaks
#define TWISTER_TORSION_WAVE 7 // cycles of that wave over the period, under an envelope that closes once
#define TWISTER_SPEED 30.0 // table units per second, half the original's one a frame

#define TWISTER_IMAGE_SIZE 256 // the picture is square and wraps both ways
#define TWISTER_IMAGE_FACE (TWISTER_IMAGE_SIZE / 4) // texels a face carries, the picture going once round the four
#define TWISTER_IMAGE_SCROLL 2.0 // texels the picture climbs per table unit, four whole pictures over the period

//
// One scanline of one face, half-open in x
//
// texels is the picture row the scanline reads, and base the first of the IMAGE_FACE texels
// this face carries. The face carries all of them whatever it measures on screen, so u steps
// by IMAGE_FACE / (right - left) per pixel. Clipping the left edge advances u to the texel
// that pixel would have had rather than restarting the span, though the column is narrower
// than the screen and the clamp does not bite. A face turned edge on has right <= left and
// covers nothing, which is the silhouette test.
//
// base is a whole quarter of the picture and u runs less than a quarter past it, so the walk
// stays inside the row and needs no wrapping of its own.
//
void DrawSpan(int left, int right, int y, unsigned char *texels, int base)
{
	if (right <= left) {
		return;
	}

	float du = (float)TWISTER_IMAGE_FACE / (right - left);

	int x0 = MAX(left, 0);
	int x1 = MIN(right, RETRO_WIDTH);
	float u = base + (x0 - left) * du;

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = x0; x < x1; x++, u += du) {
		row[x] = texels[(int)u];
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TWISTER_SPEED, TWISTER_PERIOD);
	double torsion = TWISTER_TORSION * SIN(phase * TWISTER_TORSION_WAVE * TWISTER_CYCLE) * COS(phase * TWISTER_CYCLE);

	// Move scroll
	double scroll = phase * TWISTER_IMAGE_SCROLL;

	unsigned char *image = RETRO_ImageData();

	// Draw column
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double index = y * torsion + phase;
		int v = WRAP(y + scroll, TWISTER_IMAGE_SIZE);
		unsigned char *texels = image + v * TWISTER_IMAGE_SIZE;

		double angle = index * TWISTER_TURNS * TWISTER_CYCLE;
		double sin_radius = TWISTER_RADIUS * SIN(angle);
		double cos_radius = TWISTER_RADIUS * COS(angle);
		int corner_x[4] = {
			(int)lround(TWISTER_CENTER_X - cos_radius),
			(int)lround(TWISTER_CENTER_X + sin_radius),
			(int)lround(TWISTER_CENTER_X + cos_radius),
			(int)lround(TWISTER_CENTER_X - sin_radius),
		};

		int face = 0;
		for (int corner = 1; corner < 4; corner++) {
			if (corner_x[corner] < corner_x[face]) {
				face = corner;
			}
		}

		// The two faces turned toward the viewer, leftmost to nearest to rightmost, each
		// carrying the quarter of the picture that its own corner starts
		DrawSpan(corner_x[face], corner_x[(face + 1) & 3], y, texels, face * TWISTER_IMAGE_FACE);
		DrawSpan(corner_x[(face + 1) & 3], corner_x[(face + 2) & 3], y, texels, ((face + 1) & 3) * TWISTER_IMAGE_FACE);
	}
}

void DEMO_Initialize(void)
{
	// Init image, and take its palette. The picture is drawn as it is, so the palette
	// it was quantized against is the only one that gives back the picture
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);
}
