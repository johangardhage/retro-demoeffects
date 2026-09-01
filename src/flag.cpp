//
// Waving flag
//
// Lit cloth in the parameter frame: x along the hoist–fly axis, y down the
// hoist (screen y), z toward the viewer. The surface is
//
//   h(x, y, t) = A(x) * sum_i a_i sin(2π (x/λx_i − t SPEED/λx_i + y/λy_i + φ_i))
//
// A(x) = AMPLITUDE sin(π x / 2 W) is the held-free fundamental (A(0) = 0,
// A'(W) = 0). All modes share the same phase speed SPEED, so a crest keeps
// its shape along x. Three things are taken from h:
//
//   N = (−hx, −hy, 1) / |…|          height-field normal
//   s = ∫ sqrt(1 + hx²) dx           arc length along x only
//   up = y − swing · A/AMP − tilt h  sheared screen row, not arc length in y
//
// The print is inextensible along x (the fly pulls in where the cloth
// folds) and shears with the silhouette in y. Lighting is Blinn-Phong of
// a directional sun L and an orthographic V = (0, 0, 1). L is (−5, −2, 4)
// normalised: 37° above the cloth, from the left. Swing does not rotate N.
// Ambient occlusion 1/(1 + k max(hxx, 0)) shuts out the sky in valleys
// only; the sun is not scaled.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

// Flag of Sweden, SFS 1982:269. The proportions are 16:10, divided 5:2:9 along the flag and
// 4:2:4 across it, which puts the cross off centre towards the hoist and makes both arms the
// same thickness. 256 by 160 is exactly 16:10, so none of that has to be compromised.
#define FLAG_WIDTH 256
#define FLAG_HEIGHT 160
#define FLAG_LEFT ((RETRO_WIDTH - FLAG_WIDTH) / 2)
#define FLAG_TOP ((RETRO_HEIGHT - FLAG_HEIGHT) / 2)
#define CROSS_LEFT (FLAG_WIDTH * 5 / 16)
#define CROSS_RIGHT (FLAG_WIDTH * 7 / 16)
#define CROSS_TOP (FLAG_HEIGHT * 4 / 10)
#define CROSS_BOTTOM (FLAG_HEIGHT * 6 / 10)

// A shade ramp per color of the flag, the two splitting the palette evenly so
// that every entry carries a shade
#define FLAG_COLORS (RETRO_COLORS / 2)
#define BLUE_SHADES 0
#define YELLOW_SHADES FLAG_COLORS

#define CLOTH_AMPLITUDE 22.0 // fold depth at the fly, in pixels
#define CLOTH_SPEED 85 // pixels of wave travel per second
#define CLOTH_WAVES 7
#define CLOTH_OCCLUSION 4.0 // how far a crease shuts out the sky, per unit of curvature
#define CLOTH_TILT 0.45 // the flag plane leans out of the screen, so a fold carries the
                           // cloth across the picture as well as into it
#define CLOTH_SWAY 16 // pixels the fly rises and falls as the flag swings on its halyard
#define CLOTH_SWAYPERIOD 4.3 // seconds for one swing

// Travel at which every mode is back on the crest it started from: the lcm of the
// wavelengths 240, 155, 100, 65, 42, 27, 18 = 2^4·3^3·5^2·7·13·31 pixels.
#define CLOTH_PERIOD (16.0 * 27 * 25 * 7 * 13 * 31)

// Sunlight, so one direction serves the whole surface. (−5, −2, 4) normalised:
// 37° above the cloth, from the left, well off the viewer's axis, so the
// shading changes as the surface tilts.
#define LIGHT_DIRX -0.74536
#define LIGHT_DIRY -0.29814
#define LIGHT_DIRZ 0.59629

#define MATERIAL_AMBIENT 0.34 // cloth is lit by the whole sky, not only by the sun
#define MATERIAL_DIFFUSE 0.60
#define MATERIAL_SPECULAR 0.16 // cloth has a broad sheen, not a highlight
#define MATERIAL_SHININESS 12 // fixed: the specular is unrolled as x⁴·x⁴·x⁴

//
// One mode of the surface
//
// Long modes carry the shape; short ones are relief. Wavelengths share no common
// factor so crests never lock, and amplitudes fall off so the short modes texture
// the surface without deforming it.
//
struct ClothWave {
	double wavelengthx; // along the flag, in pixels
	double wavelengthy; // across it, so how far a crest leans over the height of the flag
	double amplitude; // relative to the others, summing to 1
	double phase; // in turns, so the modes do not all start from the same crest
};

ClothWave ClothWaves[CLOTH_WAVES] = {
	{ 240, 430, 0.293, 0.00 },
	{ 155, -310, 0.208, 0.37 },
	{ 100, 720, 0.146, 0.71 },
	{ 65, -225, 0.104, 0.13 },
	{ 42, 880, 0.098, 0.89 },
	{ 27, -165, 0.083, 0.52 },
	{ 18, 470, 0.068, 0.28 },
};

// Phase is linear in x, so one pixel is a fixed rotation of the unit vector
// (sin θ, cos θ) by Δ = 2π / λx:
//
//   (sin, cos)' = (sin Δ, cos Δ) applied as a 2D rotation
//
struct ClothPhasor {
	double sine, cosine;
};

double ClothStepSin[CLOTH_WAVES];
double ClothStepCos[CLOTH_WAVES];

// 2π / λ for each mode, along the flag and across it. Constant, and the inner
// loop runs once per mode per pixel, so they are divided out once at startup
// rather than 2 · CLOTH_WAVES times for every pixel of every frame.
double ClothTurnX[CLOTH_WAVES];
double ClothTurnY[CLOTH_WAVES];

// The envelope A(x) and its first two derivatives, which depend only on the distance from
// the mast and so are the same in every row and every frame
double ClothEnvelope[FLAG_WIDTH];
double ClothEnvelopeSlope[FLAG_WIDTH];
double ClothEnvelopeCurve[FLAG_WIDTH];

double HalfwayX, HalfwayY, HalfwayZ;

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Calculate phase. Each wraps on its own exact period, so neither grows
	// without bound: the swing closes on CLOTH_SWAYPERIOD, the wave phases on
	// CLOTH_PERIOD pixels of travel.
	double sway = fmod(time, CLOTH_SWAYPERIOD);
	double travel = fmod(time * CLOTH_SPEED, CLOTH_PERIOD);

	// Rigid swing about the mast: up = y - swing * A(x)/AMPLITUDE. Lighting is unchanged.
	double swing = CLOTH_SWAY * sin(2 * M_PI * sway / CLOTH_SWAYPERIOD);

	// Draw flag. The background is the cleared framebuffer: both ramps start at
	// black, so index 0 is black and nothing has to be painted behind the cloth.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int across = y - FLAG_TOP;

		// Start each mode at the left edge of the flag, then turn it pixel by pixel
		ClothPhasor phasor[CLOTH_WAVES];
		for (int i = 0; i < CLOTH_WAVES; i++) {
			double phase = 2 * M_PI * (-travel / ClothWaves[i].wavelengthx + across / ClothWaves[i].wavelengthy + ClothWaves[i].phase);

			phasor[i].sine = sin(phase);
			phasor[i].cosine = cos(phase);
		}

		// Cloth does not stretch. The print is sampled along arc length
		// s = ∫ sqrt(1 + (dh/dx)^2) dx, so the cross bends where the surface folds.
		double material = 0;

		for (int x = 0; x < FLAG_WIDTH; x++) {
			// All modes travel at CLOTH_SPEED, so the medium is non-dispersive and a
			// crest keeps its shape. Derivatives are exact by the product rule.
			// dh/dy uses the same cosines, so it is accumulated before the phasor
			// advances to the next pixel.
			double waves = 0;
			double waveslope = 0;
			double wavecurve = 0;
			double slopey = 0;

			for (int i = 0; i < CLOTH_WAVES; i++) {
				double turn = ClothTurnX[i];

				waves += ClothWaves[i].amplitude * phasor[i].sine;
				waveslope += ClothWaves[i].amplitude * turn * phasor[i].cosine;
				wavecurve -= ClothWaves[i].amplitude * turn * turn * phasor[i].sine;
				slopey += ClothWaves[i].amplitude * ClothTurnY[i] * phasor[i].cosine;

				double sine = phasor[i].sine * ClothStepCos[i] + phasor[i].cosine * ClothStepSin[i];
				double cosine = phasor[i].cosine * ClothStepCos[i] - phasor[i].sine * ClothStepSin[i];

				phasor[i].sine = sine;
				phasor[i].cosine = cosine;
			}

			// Product rule: h = A * w, so h' = A' w + A w' and h'' = A'' w + 2 A' w' + A w''.
			double slopex = ClothEnvelopeSlope[x] * waves + ClothEnvelope[x] * waveslope;
			double curvex = ClothEnvelopeCurve[x] * waves + 2 * ClothEnvelopeSlope[x] * waveslope + ClothEnvelope[x] * wavecurve;
			slopey *= ClothEnvelope[x];

			double along = material;
			material += sqrt(1 + slopex * slopex);

			// Past the fly the flag has run out, and the swing can carry a row off the top
			// or the bottom. Either way the background stands.
			double height = ClothEnvelope[x] * waves;
			double up = across - swing * ClothEnvelope[x] / CLOTH_AMPLITUDE - CLOTH_TILT * height;
			if (along >= FLAG_WIDTH || up < 0 || up >= FLAG_HEIGHT) {
				continue;
			}

			// z = h(x, y), so N = (-dh/dx, -dh/dy, 1) / |...|
			double inverselength = 1.0 / sqrt(slopex * slopex + slopey * slopey + 1);
			double normalx = -slopex * inverselength;
			double normaly = -slopey * inverselength;
			double normalz = inverselength;

			double lambert = CLAMP01(normalx * LIGHT_DIRX + normaly * LIGHT_DIRY + normalz * LIGHT_DIRZ);

			// Blinn-Phong. L and V are fixed, so H = normalize(L + V) is formed once.
			double specular = 0;
			if (lambert > 0) {
				double normalhalfway = CLAMP01(normalx * HalfwayX + normaly * HalfwayY + normalz * HalfwayZ);

				// (N·H)^12 by squaring: x², x⁴, x⁸ · x⁴. Four multiplies rather
				// than a call to pow for an exponent known at compile time.
				double squared = normalhalfway * normalhalfway;
				double fourth = squared * squared;
				specular = fourth * fourth * fourth;
			}

			// Ambient occlusion from curvature: only the sky is shut out, not the sun.
			//   occ = 1 / (1 + k * max(d²h/dx², 0))
			double occlusion = 1.0 / (1.0 + CLOTH_OCCLUSION * MAX(0.0, curvex));

			double intensity = MATERIAL_AMBIENT * occlusion + MATERIAL_DIFFUSE * lambert + MATERIAL_SPECULAR * specular;
			int shade = (FLAG_COLORS - 1) * CLAMP01(intensity) + 0.5;

			// The cross is tested in (along, up): arc length along the cloth,
			// sheared screen row across it. Not screen (x, y).
			bool yellow = (along >= CROSS_LEFT && along < CROSS_RIGHT) || (up >= CROSS_TOP && up < CROSS_BOTTOM);

			buffer[y * RETRO_WIDTH + FLAG_LEFT + x] = (yellow ? YELLOW_SHADES : BLUE_SHADES) + shade;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette, a shade ramp per color of the flag. Shade is reflected light, so
	// the ramps start at black and an unlit fold goes dark rather than merely desaturated.
	RETRO_CreateGradientPalette(BLUE_SHADES, BLUE_SHADES + FLAG_COLORS, RETRO_BLACK, RETRO_CERULEAN);
	RETRO_CreateGradientPalette(YELLOW_SHADES, YELLOW_SHADES + FLAG_COLORS, RETRO_BLACK, RETRO_GOLD);

	// A(x) = AMPLITUDE sin(π x / 2 W) and its first two derivatives. Fundamental
	// mode of a membrane held at x=0 and free at x=W.
	for (int x = 0; x < FLAG_WIDTH; x++) {
		double along = M_PI * x / (2 * FLAG_WIDTH);
		double turn = M_PI / (2 * FLAG_WIDTH);

		ClothEnvelope[x] = CLOTH_AMPLITUDE * sin(along);
		ClothEnvelopeSlope[x] = CLOTH_AMPLITUDE * turn * cos(along);
		ClothEnvelopeCurve[x] = -CLOTH_AMPLITUDE * turn * turn * sin(along);
	}

	// Init the turn each mode makes over one pixel, along the flag and across it
	for (int i = 0; i < CLOTH_WAVES; i++) {
		ClothTurnX[i] = 2 * M_PI / ClothWaves[i].wavelengthx;
		ClothTurnY[i] = 2 * M_PI / ClothWaves[i].wavelengthy;

		ClothStepSin[i] = sin(ClothTurnX[i]);
		ClothStepCos[i] = cos(ClothTurnX[i]);
	}

	// Init halfway vector, H = normalize(L + V), with the viewer at (0, 0, 1)
	HalfwayX = LIGHT_DIRX;
	HalfwayY = LIGHT_DIRY;
	HalfwayZ = LIGHT_DIRZ + 1;

	double inverselength = 1.0 / sqrt(HalfwayX * HalfwayX + HalfwayY * HalfwayY + HalfwayZ * HalfwayZ);

	HalfwayX *= inverselength;
	HalfwayY *= inverselength;
	HalfwayZ *= inverselength;
}
