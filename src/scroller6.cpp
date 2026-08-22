//
// Star Wars-style perspective text crawl
//
// A page of centred 16x16 text is built at startup. During rendering it is
// sampled as a plane receding toward a horizon: rows become narrower and
// more compressed as they travel up the screen.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944
#define PAGE_WIDTH 288
#define LINE_HEIGHT 20
#define HORIZON_Y 42
#define BOTTOM_Y RETRO_HEIGHT
#define CRAWL_SPEED 22.0
#define PERSPECTIVE_DEPTH 3600.0

static const char *crawl_text[] = {
	"RETRO DEMOEFFECTS",
	"...",
	"",
};

static constexpr int PAGE_LINES = sizeof(crawl_text) / sizeof(crawl_text[0]);
static constexpr int PAGE_HEIGHT = PAGE_LINES * LINE_HEIGHT;

unsigned char page[PAGE_WIDTH * PAGE_HEIGHT];

void DEMO_Render(double deltatime)
{
	static double phase = -FONT_HEIGHT;
	phase += deltatime * CRAWL_SPEED;
	if (phase > PAGE_HEIGHT + 180) {
		phase = -FONT_HEIGHT;
	}

	for (int y = HORIZON_Y + 1; y < BOTTOM_Y; y++) {
		double distance = y - HORIZON_Y;
		double scale = distance / (BOTTOM_Y - HORIZON_Y);
		double source_y = phase - PERSPECTIVE_DEPTH / distance;
		int sy = (int)source_y;
		if (sy < 0 || sy >= PAGE_HEIGHT) {
			continue;
		}

		int half_width = (int)(PAGE_WIDTH * scale / 2.0);
		if (half_width < 1) {
			continue;
		}
		int left = RETRO_WIDTH / 2 - half_width;
		int right = RETRO_WIDTH / 2 + half_width;

		for (int x = MAX(left, 0); x < MIN(right, RETRO_WIDTH); x++) {
			double u = (x - RETRO_WIDTH / 2) / scale + PAGE_WIDTH / 2.0;
			int sx = (int)u;
			if (sx < 0 || sx >= PAGE_WIDTH) {
				continue;
			}

			unsigned char color = page[sy * PAGE_WIDTH + sx];
			if (color != 0) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx", true);
	unsigned char *font = RETRO_ImageData();

	for (int line = 0; line < PAGE_LINES; line++) {
		int length = 0;
		while (crawl_text[line][length] != '\0') {
			length++;
		}
		int start_x = (PAGE_WIDTH - length * FONT_WIDTH) / 2;
		int start_y = line * LINE_HEIGHT;

		for (int letter = 0; letter < length; letter++) {
			unsigned char character = crawl_text[line][letter];
			if (character < 32 || character >= 91) {
				continue;
			}
			unsigned char *src = font + (character - 32) * FONT_WIDTH;
			int dst_x = start_x + letter * FONT_WIDTH;

			for (int fy = 0; fy < FONT_HEIGHT; fy++) {
				for (int fx = 0; fx < FONT_WIDTH; fx++) {
					int px = dst_x + fx;
					if (px >= 0 && px < PAGE_WIDTH) {
						page[(start_y + fy) * PAGE_WIDTH + px] =
							src[fy * IMAGE_WIDTH + fx];
					}
				}
			}
		}
	}
}
