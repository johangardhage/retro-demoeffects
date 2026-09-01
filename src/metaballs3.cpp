//
// 3D metaballs
//
// The same 1/r² field as metaballs.cpp, in 3-space. At a point p
//
//   F(p) = sum_i  T R_i² / |p − c_i|²
//
// the blob is the superlevel set F ≥ T. One ball alone meets T on the
// sphere of radius R_i. Where fields overlap, F exceeds T between them
// and the surfaces merge. Two equal balls stay one component while the
// midpoint still meets T, which is D ≤ R√8; the orbits run wider than
// that, so the blob splits. n coincident balls of equal R meet T on the
// sphere of radius R√n, so a bounding sphere of radius sqrt(sum R_k²)
// around each centre is exact in that worst case and conservative
// otherwise. |p−c|² is floored at 10⁻⁴ so a sample on a centre does
// not divide by zero.
//
// A pixel is a pinhole ray from the eye through the pixel centre. y
// grows down, z along the view, matching RETRO_ProjectVertex: the eye
// sits at (W/2, H/2, −EYE) and the screen is z = 0, so
//
//   D = normalize((x + 1/2 − W/2,  y + 1/2 − H/2,  EYE))
//   p(t) = O + t D
//
// |p − c|² is a quadratic in t. Along a ray it is stepped by forward
// differences, the same device the 2D scanline uses in x:
//
//   r²(t+h) − r²(t) = 2h (D·(O−c) + t) + h²
//
// and that first difference itself grows by 2h² each step. The walk
// starts at the first bounding-sphere entry and stops at the last
// exit. The first sample with F ≥ T is the crossing; eight bisections
// of that last step put the hit on the isosurface.
//
// The outward normal is −∇F. ∇(1/r²) = −2 (p−c) / r⁴, so
//
//   ∇F = sum_i −2 T R_i² (p − c_i) / |p − c_i|⁴
//   N  = −∇F / |∇F|
//
// L = (0, 0, −1) is the same headlight as phongcube.cpp. The shade is
// ShadeFromLambert(max(N·L, 0)) into a plastic Phong ramp.
//
// Each centre rides its own 3-axis Lissajous. Whole-number rates keep
// every orbit closed on the same 2π of phase.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define NUM_BALLS 4
#define THRESHOLD 50 // F = T on the sphere of radius R around one ball
#define EYE 250 // pinhole focal length, the library's eyedistance, in pixels
#define MARCH_STEP 2.0f // world pixels a field sample advances along the ray
#define MARCH_MAX 160 // samples along one ray, including the first
#define BISECT_STEPS 8 // halvings of the crossing step
#define ORBIT_SPEED 1.0 // radians of the base orbit per second; the rates below multiply it

struct MetaBall {
	float x, y, z;
	float bound;
} Balls[NUM_BALLS];

float Charge[NUM_BALLS];

static const float BallRadius[NUM_BALLS] = { 36, 32, 28, 34 };

float Field(float x, float y, float z)
{
	float sum = 0;
	for (int i = 0; i < NUM_BALLS; i++) {
		float dx = x - Balls[i].x;
		float dy = y - Balls[i].y;
		float dz = z - Balls[i].z;
		float r2 = MAX(dx * dx + dy * dy + dz * dz, 0.0001f);
		sum += Charge[i] / r2;
	}
	return sum;
}

// Outward unit normal N = −∇F / |∇F|. ∇F = 0 only at a critical point of F,
// and the isosurface meets one only at the instant a blob splits or merges,
// when it passes through the saddle between two centres. The fallback for that
// instant is the headlight itself, which shades the pixel fully lit.
void FieldNormal(float x, float y, float z, float *nx, float *ny, float *nz)
{
	float gx = 0;
	float gy = 0;
	float gz = 0;
	for (int i = 0; i < NUM_BALLS; i++) {
		float dx = x - Balls[i].x;
		float dy = y - Balls[i].y;
		float dz = z - Balls[i].z;
		float r2 = MAX(dx * dx + dy * dy + dz * dz, 0.0001f);
		float g = -2 * Charge[i] / (r2 * r2);
		gx += g * dx;
		gy += g * dy;
		gz += g * dz;
	}

	float glen2 = gx * gx + gy * gy + gz * gz;
	if (glen2 < 1.0e-12f) {
		*nx = 0;
		*ny = 0;
		*nz = -1;
		return;
	}

	float inv = 1.0 / sqrt(glen2);
	*nx = -gx * inv;
	*ny = -gy * inv;
	*nz = -gz * inv;
}

// Unit-D ray against a sphere. tEnter can be negative when the eye is inside.
bool RaySphere(float ox, float oy, float oz, float dx, float dy, float dz, float cx, float cy, float cz, float radius, float *tenter, float *tleave)
{
	float ocx = ox - cx;
	float ocy = oy - cy;
	float ocz = oz - cz;
	float b = dx * ocx + dy * ocy + dz * ocz;
	float c = ocx * ocx + ocy * ocy + ocz * ocz - radius * radius;
	float disc = b * b - c;
	if (disc < 0) {
		return false;
	}

	float s = sqrt(disc);
	*tenter = -b - s;
	*tleave = -b + s;
	return *tleave > 0;
}

void ShadeHit(int x, int y, float px, float py, float pz)
{
	float nx, ny, nz;
	FieldNormal(px, py, pz, &nx, &ny, &nz);

	// L = (0, 0, −1)
	float lambert = MAX(-nz, 0.0f);
	float intensity = RETRO_ShadeFromLambert(lambert);
	int color = RETRO_PHONG_OFFSET + RETRO_PHONG_SHADES * intensity;
	RETRO_PutPixel(x, y, CLAMP(color, RETRO_PHONG_OFFSET, RETRO_COLORS));
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * ORBIT_SPEED, 2 * M_PI);

	// Each ball a 3-axis Lissajous. Whole-number rates close on 2π of phase.
	static const float amplitudex[NUM_BALLS] = { 88, 28, -82, 60 };
	static const float amplitudey[NUM_BALLS] = { 24, 70, 50, -68 };
	static const float amplitudez[NUM_BALLS] = { 36, -42, 48, 28 };
	static const int ratex[NUM_BALLS] = { 1, 2, 3, 1 };
	static const int ratey[NUM_BALLS] = { 2, 1, 2, 3 };
	static const int ratez[NUM_BALLS] = { 3, 2, 1, 2 };
	static const float offsetx[NUM_BALLS] = { 0.0, 1.2, 0.7, 2.5 };
	static const float offsety[NUM_BALLS] = { 0.4, 0.0, 1.8, 0.5 };
	static const float offsetz[NUM_BALLS] = { 1.0, 2.1, 0.3, 1.6 };

	float cx = RETRO_WIDTH / 2.0f;
	float cy = RETRO_HEIGHT / 2.0f;
	float cz = 0;

	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].x = cx + amplitudex[i] * sin(ratex[i] * phase + offsetx[i]);
		Balls[i].y = cy + amplitudey[i] * sin(ratey[i] * phase + offsety[i]);
		Balls[i].z = cz + amplitudez[i] * sin(ratez[i] * phase + offsetz[i]);
	}

	float ox = cx;
	float oy = cy;
	float oz = -EYE;

	// Draw balls. A pixel whose ray misses every bounding sphere is left
	// cleared. |p − c|² is a quadratic in t, so it is stepped by forward
	// differences: d(t+h) − d(t) = 2h (D·(O−c) + t) + h², which itself
	// grows by 2h² each sample.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float py = y + 0.5f - cy;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float px = x + 0.5f - cx;
			float pz = EYE;
			float invlen = 1.0 / sqrt(px * px + py * py + pz * pz);
			float dx = px * invlen;
			float dy = py * invlen;
			float dz = pz * invlen;

			float tmin = 1.0e9f;
			float tmax = 0;
			bool hitbound = false;
			for (int i = 0; i < NUM_BALLS; i++) {
				float tenter, tleave;
				if (!RaySphere(ox, oy, oz, dx, dy, dz, Balls[i].x, Balls[i].y, Balls[i].z, Balls[i].bound, &tenter, &tleave)) {
					continue;
				}
				if (tenter < 0) {
					tenter = 0;
				}
				if (tleave > tenter) {
					tmin = MIN(tmin, tenter);
					tmax = MAX(tmax, tleave);
					hitbound = true;
				}
			}
			if (!hitbound) {
				continue;
			}

			float h = MARCH_STEP;
			float twostep2 = 2 * h * h;

			float distancesquared[NUM_BALLS];
			float slope[NUM_BALLS];
			for (int i = 0; i < NUM_BALLS; i++) {
				float ocx = ox - Balls[i].x;
				float ocy = oy - Balls[i].y;
				float ocz = oz - Balls[i].z;
				float doc = dx * ocx + dy * ocy + dz * ocz;
				distancesquared[i] = ocx * ocx + ocy * ocy + ocz * ocz + 2 * tmin * doc + tmin * tmin;
				slope[i] = 2 * h * (doc + tmin) + h * h;
			}

			float sum = 0;
			for (int i = 0; i < NUM_BALLS; i++) {
				sum += Charge[i] / MAX(distancesquared[i], 0.0001f);
			}

			if (sum >= THRESHOLD) {
				ShadeHit(x, y, ox + tmin * dx, oy + tmin * dy, oz + tmin * dz);
				continue;
			}

			float t = tmin;
			int steps = 0;
			while (t < tmax && steps < MARCH_MAX) {
				for (int i = 0; i < NUM_BALLS; i++) {
					distancesquared[i] += slope[i];
					slope[i] += twostep2;
				}
				t += h;
				steps++;

				float next = 0;
				for (int i = 0; i < NUM_BALLS; i++) {
					next += Charge[i] / MAX(distancesquared[i], 0.0001f);
				}

				if (next >= THRESHOLD) {
					float lo = t - h;
					float hi = t;
					for (int k = 0; k < BISECT_STEPS; k++) {
						float mid = 0.5f * (lo + hi);
						if (Field(ox + mid * dx, oy + mid * dy, oz + mid * dz) < THRESHOLD) {
							lo = mid;
						} else {
							hi = mid;
						}
					}
					float thit = 0.5f * (lo + hi);
					ShadeHit(x, y, ox + thit * dx, oy + thit * dy, oz + thit * dz);
					break;
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreatePlasticPhongPalette(30, RETRO_CYAN);

	// Charge of each ball, and the coincident-equal bound sqrt(sum R²)
	float sumr2 = 0;
	for (int i = 0; i < NUM_BALLS; i++) {
		Charge[i] = THRESHOLD * BallRadius[i] * BallRadius[i];
		sumr2 += BallRadius[i] * BallRadius[i];
	}
	float bound = sqrt(sumr2);
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].bound = bound;
	}
}
