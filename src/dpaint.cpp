//
// Paint
//
// A persistent canvas (DEMO_Render2, so nothing is cleared). The mouse is
// a round or square brush (four sizes each), or one of two dotted brushes,
// picked at the top of the right
// panel; left writes the selected palette color, right writes 0. Startup
// is absolute (the logical point is the window mapping). The relative
// branch, if enabled, moves by 0.2 · (dx, dy) so a fast flick does not
// jump the brush; the point is a float so those fractions accumulate.
// Leaving the 640×480 box clamps the point to the last pixel and warps
// the OS cursor back.
//
// The menu bar and right-side tools come from a DeluxePaint II screenshot;
// the palette swatches are drawn dynamically. The brush is
// clipped to stay off both, though the pointer still moves freely over
// them so the panel's own controls are reachable. Brush strokes persist in
// Canvas rather than RETRO.framebuffer, which is rebuilt from Canvas every
// frame; that's what lets the Amiga mouse pointer draw on top without
// leaving a trail of itself behind.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 480

#include "lib/retro.h"
#include "lib/retrogfx.h"
#include "lib/retromain.h"
#include "lib/retromouse.h"
#include "lib/retropalette.h"

#define MOUSE_ACCEL 0.2 // relative-mode step per window pixel

// The Amiga .cur sources are 32x32 with hotspot (10,10); RETRO_DrawSprite
// centers on its (x,y), so the sprite must be offset by (center - hotspot)
// to land the hotspot, not the sprite's center, on the mouse position.
#define POINTER_SIZE 32
#define POINTER_HOTSPOT_OFFSET (POINTER_SIZE / 2 - 10)

// Canvas area, clear of the chrome (matches dpaint_640x480.pcx)
#define CANVAS_TOP 12
#define CANVAS_RIGHT 592

// Brush-size selector: round, square, then two dotted brushes, at the top
// of the panel (matches dpaint_640x480.pcx)
#define BRUSH_SELECTOR_TOP CANVAS_TOP
#define BRUSH_SELECTOR_SPLIT 30
#define BRUSH_SPRAY_TOP 40
#define BRUSH_SELECTOR_BOTTOM 52

// Unequal icon slots: full row height, with widths following the brush layout.
// Share horizontal edges between highlighting and hit testing.
static const int BrushColumnEdges[2][5] = {
	{ 594, 602, 612, 624, 640 },
	{ 594, 608, 620, 630, 640 }
};

enum BrushKind { BRUSH_ROUND, BRUSH_SQUARE, BRUSH_SPRAY };
static const struct { int left, top, right, bottom; } SprayBounds[2] = {
	{ 594, 40, 615, 51 }, { 618, 38, 639, 51 }
};

// Nine rows of two tool buttons, separated by two-pixel black rules.
#define TOOL_TOP 52
#define TOOL_CELL_SIZE 24
#define TOOL_ROWS 9
#define TOOL_COLUMNS 2
#define TOOL_DEFAULT_SELECTION 1 // freehand

// Color swatches: base colors on the left, bright variants on the right.
// Reserve separate indices so paint colors do not recolor the interface/pointers.
#define PAINT_PALETTE_OFFSET 32
#define SWATCH_LEFT (CANVAS_RIGHT + 1)
#define SWATCH_ROWS 8
#define SWATCH_COL_SPLIT 617

// Exact row boundaries from the asset; the first row is 22 pixels tall.
static const int SwatchRowEdges[SWATCH_ROWS + 1] = { 311, 333, 354, 375, 396, 417, 438, 459, 480 };

static const unsigned char BrushSizes[4] = { 0, 2, 4, 6 }; // radius/half-width in pixels

static unsigned char SwatchColor(int row, int col)
{
	return PAINT_PALETTE_OFFSET + row + col * SWATCH_ROWS;
}

unsigned char Canvas[RETRO_WIDTH * RETRO_HEIGHT];

static void DrawSwatches(void)
{
	for (int row = 0; row < SWATCH_ROWS; row++) {
		RETRO_DrawRectangle(SWATCH_LEFT, SwatchRowEdges[row], SWATCH_COL_SPLIT - 1,
			SwatchRowEdges[row + 1] - 1, SwatchColor(row, 0));
		RETRO_DrawRectangle(SWATCH_COL_SPLIT, SwatchRowEdges[row], RETRO_WIDTH - 1,
			SwatchRowEdges[row + 1] - 1, SwatchColor(row, 1));
	}
}

// Only invert the button interior, keeping the grid lines intact.
static void InvertToolButton(int tool, unsigned char *buffer)
{
	int x0 = CANVAS_RIGHT + (tool % TOOL_COLUMNS) * TOOL_CELL_SIZE + 2;
	int y0 = TOOL_TOP + (tool / TOOL_COLUMNS) * TOOL_CELL_SIZE + 2;
	for (int yy = y0; yy < y0 + TOOL_CELL_SIZE - 2; yy++) {
		for (int xx = x0; xx < x0 + TOOL_CELL_SIZE - 2; xx++) {
			unsigned char &color = buffer[yy * RETRO_WIDTH + xx];
			if (color <= 1) color = 1 - color;
		}
	}
}

// Outlines the given swatch cell on the framebuffer (transient, like the
// pointer, so it moves with selection instead of getting baked into Canvas)
static void DrawSwatchSelection(int row, int col)
{
	int x0 = col == 0 ? SWATCH_LEFT : SWATCH_COL_SPLIT;
	int x1 = col == 0 ? SWATCH_COL_SPLIT - 1 : RETRO_WIDTH - 1;
	int y0 = SwatchRowEdges[row];
	int y1 = SwatchRowEdges[row + 1] - 1;

	RETRO_Palette swatch = RETRO_GetColor(SwatchColor(row, col));
	int brightness = 299 * swatch.r + 587 * swatch.g + 114 * swatch.b;
	unsigned char color = PAINT_PALETTE_OFFSET + (brightness >= 128000 ? 0 : 15);
	RETRO_DrawRectangle(x0, y0, x1, y0, color);
	RETRO_DrawRectangle(x0, y1, x1, y1, color);
	RETRO_DrawVline(x0, y0, y1, color);
	RETRO_DrawVline(x1, y0, y1, color);
}

// Marks the selected brush cell DPaint's own way: invert its icon (black
// glyph on white) to white-on-black, rather than a box. Framebuffer only, like
// DrawSwatchSelection, so it moves with selection instead of getting baked
// into Canvas.
static void InvertBrushCell(BrushKind kind, int col)
{
	const int *edges = BrushColumnEdges[kind == BRUSH_SQUARE ? 1 : 0];
	int x0 = kind == BRUSH_SPRAY ? SprayBounds[col].left : edges[col];
	int x1 = kind == BRUSH_SPRAY ? SprayBounds[col].right : edges[col + 1] - 1;
	int y0 = kind == BRUSH_SPRAY ? SprayBounds[col].top : kind == BRUSH_SQUARE ? 30 : 14;
	int y1 = kind == BRUSH_SPRAY ? SprayBounds[col].bottom : kind == BRUSH_SQUARE ? 39 : 27;

	for (int yy = y0; yy <= y1; yy++) {
		for (int xx = x0; xx <= x1; xx++) {
			unsigned char c = RETRO_GetPixel(xx, yy);
			if (c <= 1) {
				RETRO_PutPixel(xx, yy, 1 - c);
			}
		}
	}
}

// Stamps a dotted brush, or fills a round/square brush of the given size into Canvas,
// clamped to the canvas area so a brush near the edge can't bleed onto the
// chrome even though its center already has to be off the chrome to be
// called at all.
static void PaintBrush(int cx, int cy, BrushKind kind, int size, unsigned char color)
{
	if (kind == BRUSH_SPRAY) {
		// Stamp the original icon mask, before any transient selection inversion.
		const auto &bounds = SprayBounds[size];
		int centerx = (bounds.left + bounds.right) / 2;
		int centery = (bounds.top + bounds.bottom) / 2;
		for (int sy = bounds.top; sy <= bounds.bottom; sy++) {
			for (int sx = bounds.left; sx <= bounds.right; sx++) {
				int xx = cx + sx - centerx, yy = cy + sy - centery;
				if (Canvas[sy * RETRO_WIDTH + sx] == 0 &&
					xx >= 0 && xx < CANVAS_RIGHT && yy >= CANVAS_TOP && yy < RETRO_HEIGHT) {
					Canvas[yy * RETRO_WIDTH + xx] = color;
				}
			}
		}
		return;
	}
	int y0 = MAX(cy - size, CANVAS_TOP);
	int y1 = MIN(cy + size, RETRO_HEIGHT - 1);
	int x0 = MAX(cx - size, 0);
	int x1 = MIN(cx + size, CANVAS_RIGHT - 1);
	for (int yy = y0; yy <= y1; yy++) {
		for (int xx = x0; xx <= x1; xx++) {
			if (kind == BRUSH_SQUARE || (xx - cx) * (xx - cx) + (yy - cy) * (yy - cy) <= size * size) {
				Canvas[yy * RETRO_WIDTH + xx] = color;
			}
		}
	}
}

void DEMO_Render2(double time, double deltatime)
{
	static float x = RETRO_WIDTH / 2.0, y = RETRO_HEIGHT / 2.0;
	static bool wb2pointer = false; // false = Workbench 1.3, true = Workbench 2.0
	static BrushKind brushkind = BRUSH_ROUND;
	static int brushsize = 0;
	static int brushcol = 0; // matches brushsize/brushkind above by default
	static int selectedrow = 7, selectedcol = 1; // white
	static int paintcolor = SwatchColor(selectedrow, selectedcol);
	static int selectedtool = TOOL_DEFAULT_SELECTION;

	RETRO_MouseState mouse = RETRO_GetMouseState();

	if (mouse.isrelative) {
		x += mouse.xrel * MOUSE_ACCEL;
		y += mouse.yrel * MOUSE_ACCEL;
	} else {
		x = mouse.x;
		y = mouse.y;
	}

	// Trap mouse cursor
	if (x < 0 || x > RETRO_WIDTH - 1 || y < 0 || y > RETRO_HEIGHT - 1) {
		x = CLAMPWIDTH(x);
		y = CLAMPHEIGHT(y);

		// Transform logical mouse position to window position and move mouse
		float realx, realy;
		SDL_RenderCoordinatesToWindow(RETRO.renderer, x, y, &realx, &realy);
		SDL_WarpMouseInWindow(RETRO.window, realx, realy);
	}

	// Swap Amiga pointer style
	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		wb2pointer = !wb2pointer;
	}

	// Pick a brush size/shape from the top of the panel
	if (mouse.leftbutton && (int)x >= CANVAS_RIGHT && (int)y >= BRUSH_SELECTOR_TOP && (int)y < BRUSH_SELECTOR_BOTTOM) {
		bool spray = (int)y >= BRUSH_SPRAY_TOP || ((int)x >= 618 && (int)y >= 38);
		brushkind = spray ? BRUSH_SPRAY : (int)y >= BRUSH_SELECTOR_SPLIT ? BRUSH_SQUARE : BRUSH_ROUND;
		if (spray) {
			brushcol = (int)x >= 617 ? 1 : 0;
			brushsize = brushcol;
		} else {
			const int *edges = BrushColumnEdges[brushkind == BRUSH_SQUARE ? 1 : 0];
			brushcol = 0;
			while (brushcol < 3 && (int)x >= edges[brushcol + 1]) {
				brushcol++;
			}
			// Square icons shrink left to right; round icons grow.
			brushsize = BrushSizes[brushkind == BRUSH_SQUARE ? 3 - brushcol : brushcol];
		}
	}

	// Pick a color from the palette swatches
	if (mouse.leftbutton && (int)x >= SWATCH_LEFT && (int)y >= SwatchRowEdges[0] && (int)y < SwatchRowEdges[SWATCH_ROWS]) {
		selectedrow = 0;
		while ((int)y >= SwatchRowEdges[selectedrow + 1]) {
			selectedrow++;
		}
		selectedcol = (int)x >= SWATCH_COL_SPLIT ? 1 : 0;
		paintcolor = SwatchColor(selectedrow, selectedcol);
	}

	// Tool actions are placeholders for now; clicking only changes selection.
	if (mouse.leftcount == 1 && (int)x >= CANVAS_RIGHT &&
		(int)y >= TOOL_TOP && (int)y < TOOL_TOP + TOOL_ROWS * TOOL_CELL_SIZE) {
		int col = ((int)x - CANVAS_RIGHT) / TOOL_CELL_SIZE;
		int row = ((int)y - TOOL_TOP) / TOOL_CELL_SIZE;
		selectedtool = row * TOOL_COLUMNS + col;
	}

	// Paint (into the persistent canvas, not the framebuffer), but not over
	// the menu bar or right panel
	if ((int)y >= CANVAS_TOP && (int)x < CANVAS_RIGHT) {
		if (mouse.leftbutton) {
			PaintBrush((int)x, (int)y, brushkind, brushsize, paintcolor);
		} else if (mouse.rightbutton) {
			PaintBrush((int)x, (int)y, brushkind, brushsize, 0);
		}
	}

	// Rebuild the frame from the canvas, then draw the selection indicators
	// and the pointer on top of it
	RETRO_Blit(Canvas);
	DrawSwatches();
	DrawSwatchSelection(selectedrow, selectedcol);
	InvertBrushCell(brushkind, brushcol);
	InvertToolButton(selectedtool, RETRO.framebuffer);
	RETRO_DrawSprite((int)x + POINTER_HOTSPOT_OFFSET, (int)y + POINTER_HOTSPOT_OFFSET, POINTER_SIZE, POINTER_SIZE,
		POINTER_SIZE, POINTER_SIZE, RETRO_ImageData(wb2pointer ? 2 : 1), 0);

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Load the DeluxePaint chrome (black/white UI, paint colors at 32-47) and seed
	// the persistent canvas with it. No swatch has a baked-in selection
	// border, so DrawSwatchSelection is the only one ever shown.
	RETRO_LoadImage("assets/dpaint_640x480.pcx", true);
	RETRO_Blit(RETRO_ImageData(0), RETRO_WIDTH * RETRO_HEIGHT, Canvas);

	// Load the Amiga pointer sprites. Their colors are pre-baked into
	// indices 16-19 (Workbench 1.3) and 20-22 (Workbench 2.0) so they can
	// share the chrome's palette; install just those, untouched (setpalette
	// defaults to false).
	RETRO_Image *wb13 = RETRO_LoadImage("assets/pointer_wb13_32x32.pcx");
	for (int i = 16; i <= 19; i++) {
		RETRO_SetColor(i, wb13->palette[i].r, wb13->palette[i].g, wb13->palette[i].b);
	}
	RETRO_Image *wb2 = RETRO_LoadImage("assets/pointer_wb20_32x32.pcx");
	for (int i = 20; i <= 22; i++) {
		RETRO_SetColor(i, wb2->palette[i].r, wb2->palette[i].g, wb2->palette[i].b);
	}

	for (int i = 0; i < 16; i++) {
		RETRO_SetColor(PAINT_PALETTE_OFFSET + i, RETRO_Default8bitPalette[i]);
	}

	// Set relative mouse mode
	RETRO_SetMouseMode(false);

	// Move mouse cursor to middle of screen
	float realx, realy;
	SDL_RenderCoordinatesToWindow(RETRO.renderer, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, &realx, &realy);
	SDL_WarpMouseInWindow(RETRO.window, realx, realy);
}
