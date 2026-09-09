//
// Conio
//
// Not the SDL framebuffer. This is the DOS conio shim: a 1-based text
// cell grid (gotoxy(1, 1) is the top-left), 16 palette colors, gotoxy /
// cprintf / getch. DEMO_Startup runs once through a few conio sections -
// a full-screen color chart, a window()-scoped box using textattr and
// highvideo/lowvideo/normvideo, and a getpass() prompt - pausing for a
// key between each. There is no render loop.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include <cstring>
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
	_setcursortype(_NOCURSOR);
	getch();
	_setcursortype(_NORMALCURSOR);

	// window() scopes gotoxy/clrscr/clreol/insline/delline to a sub-region -
	// the classic Borland boxed-menu/dialog pattern.
	window(20, 10, 60, 16);
	textattr(0x4F); // white on red, packed DOS attribute byte (bg<<4 | fg)
	clrscr();
	gotoxy(1, 1);
	cprintf("Windowed box");
	gotoxy(1, 2);
	highvideo();
	cprintf("highvideo: brighter");
	gotoxy(1, 3);
	lowvideo();
	cprintf("lowvideo: back to normal");
	gotoxy(1, 4);
	normvideo();
	cprintf("normvideo: default attribute");
	gotoxy(1, 6);
	cprintf("Press any key to continue");
	getch();

	window(1, 1, 80, 25);
	normvideo();
	clrscr();
	gotoxy(1, 1);
	char *password = getpass("Enter a password (not echoed): ");
	cprintf("Read %d characters\n", (int)strlen(password));
	cprintf("Press any key to continue");
	getch();

	gotoxy(1, 18);
	reset();
}
