//
// Conio
//
// Not the SDL framebuffer. This is the DOS conio shim: a 1-based text
// cell grid (gotoxy(1, 1) is the top-left), 16 palette colors, gotoxy /
// cprintf / getch. DEMO_Startup runs once, prints a color chart (one
// row per color 0..15), and waits for a key. There is no render loop.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retromain.h"
#include "lib/retroconio.h"

void DEMO_Startup(void)
{
	textbackground(CONIO_BLUE);
	clrscr();
	textcolor(CONIO_WHITE);
	gotoxy(30, 5);
	cprintf("Hello World");
	textcolor(CONIO_YELLOW);
	gotoxy(30, 6);
	cprintf("Hello World");
	for (int i = 0; i < 16; i++) {
		textcolor(i);
		gotoxy(1, 2 + i);
		cprintf("Hello World");
	}
	gotoxy(30, 10);
	cprintf("Press any key to continue");
	getch();
	gotoxy(1, 18);
	reset();
}
