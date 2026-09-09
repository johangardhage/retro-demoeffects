//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCONIO_H_
#define _RETROCONIO_H_

#include <stdio.h>  // printf, fflush, EOF
#include <unistd.h> // read, STDIN_FILENO
#include <fcntl.h>
#include <termios.h>

#define cprintf printf
#define cscanf scanf
#define cgets CONIO_cgets
#define getpass CONIO_getpass

#define gotoxy          CONIO_gotoxy
#define clrscr          CONIO_clrscr
#define window          CONIO_window
#define textcolor       CONIO_textcolor
#define textbackground  CONIO_textbackground
#define textattr        CONIO_textattr
#define highvideo       CONIO_highvideo
#define lowvideo        CONIO_lowvideo
#define normvideo       CONIO_normvideo
#define wherex          CONIO_wherex
#define wherey          CONIO_wherey
#define ungetch         CONIO_ungetch
#define getch           CONIO_getch
#define getche          CONIO_getche
#define kbhit           CONIO_kbhit
#define putch           CONIO_putch
#define putchar         CONIO_putch
#define cputs           CONIO_cputs
#define clreol          CONIO_clreol
#define insline         CONIO_insline
#define delline         CONIO_delline
#define _setcursortype  CONIO_setcursortype
#define reset           CONIO_reset

#define CONIO_BLACK        0
#define CONIO_BLUE         1
#define CONIO_GREEN        2
#define CONIO_CYAN         3
#define CONIO_RED          4
#define CONIO_MAGENTA      5
#define CONIO_BROWN        6
#define CONIO_LIGHTGRAY    7
#define CONIO_DARKGRAY     8
#define CONIO_LIGHTBLUE    9
#define CONIO_LIGHTGREEN   10
#define CONIO_LIGHTCYAN    11
#define CONIO_LIGHTRED     12
#define CONIO_LIGHTMAGENTA 13
#define CONIO_YELLOW       14
#define CONIO_WHITE        15
#define CONIO_BLINK        128

#define _NOCURSOR     0
#define _SOLIDCURSOR  1
#define _NORMALCURSOR 2

// DOS text mode is a fixed 80x25 character grid regardless of the physical
// display, and window() is defined in absolute screen coordinates against
// that grid - so the default window genuinely is 1,1,80,25, not "the current
// terminal size". CONIO_reset() does a real full-terminal clear on exit so a
// physically larger terminal isn't left with a dirty margin outside it.
inline struct {
	int fgc = CONIO_LIGHTGRAY; // current foreground, DOS attribute low nibble
	int bgc = 40;              // current background, as an ANSI SGR code
	bool blink = false;
	int wleft = 1, wtop = 1, wright = 80, wbottom = 25;
} CONIO;

// The single place that turns (fgc, bgc, blink) into the actual SGR escape;
// textcolor/textbackground/textattr/highvideo/lowvideo/normvideo all funnel
// through this so they stay consistent with each other.
inline void CONIO_apply_attr(void)
{
	static const int fg_ansi[8] = { 30, 34, 32, 36, 31, 35, 33, 37 };
	const char *bold = (CONIO.fgc & 0x08) ? "1" : "0";
	const char *bl = CONIO.blink ? ";5" : "";
	printf("\033[%s;%d;%d%sm", bold, fg_ansi[CONIO.fgc % 8], CONIO.bgc, bl);
}

// Coordinates are absolute screen coordinates and don't move the cursor or
// clear anything; they just redefine what gotoxy/clrscr/clreol/insline/
// delline treat as the current text window from here on.
inline void CONIO_window(int left, int top, int right, int bottom)
{
	CONIO.wleft = left;
	CONIO.wtop = top;
	CONIO.wright = right;
	CONIO.wbottom = bottom;
}

inline void CONIO_gotoxy(int x, int y)
{
	printf("\033[%d;%df", CONIO.wtop + y - 1, CONIO.wleft + x - 1);
}

inline void CONIO_clrscr(void)
{
	CONIO_apply_attr();
	int width = CONIO.wright - CONIO.wleft + 1;
	for (int row = CONIO.wtop; row <= CONIO.wbottom; row++) {
		printf("\033[%d;%df\033[%dX", row, CONIO.wleft, width);
	}
	printf("\033[%d;%df", CONIO.wtop, CONIO.wleft);
}

// Bounded to the window's rows via a temporary DECSTBM scrolling region, so
// a line insert/delete scrolls only the window, not the whole terminal - the
// region is reset back to full-screen immediately after so it doesn't affect
// unrelated output (cprintf's own newline scrolling, in particular).
inline void CONIO_insline(void)
{
	printf("\033[%d;%dr\033[1L\033[r", CONIO.wtop, CONIO.wbottom);
}

inline void CONIO_delline(void)
{
	printf("\033[%d;%dr\033[1M\033[r", CONIO.wtop, CONIO.wbottom);
}

inline void CONIO_setcursortype(int cur_t)
{
	switch (cur_t) {
	case _NOCURSOR:      printf("\033[?25l"); break;
	case _SOLIDCURSOR:   printf("\033[?25h\033[2 q"); break;
	case _NORMALCURSOR:  printf("\033[?25h\033[4 q"); break;
	}
}

inline void CONIO_textbackground(int color)
{
	switch (color % 16) {
	case CONIO_BLACK:          CONIO.bgc = 40; break;
	case CONIO_BLUE:           CONIO.bgc = 44; break;
	case CONIO_GREEN:          CONIO.bgc = 42; break;
	case CONIO_CYAN:           CONIO.bgc = 46; break;
	case CONIO_RED:            CONIO.bgc = 41; break;
	case CONIO_MAGENTA:        CONIO.bgc = 45; break;
	case CONIO_BROWN:          CONIO.bgc = 43; break;
	case CONIO_LIGHTGRAY:      CONIO.bgc = 47; break;
	case CONIO_DARKGRAY:       CONIO.bgc = 40; break;
	case CONIO_LIGHTBLUE:      CONIO.bgc = 44; break;
	case CONIO_LIGHTGREEN:     CONIO.bgc = 42; break;
	case CONIO_LIGHTCYAN:      CONIO.bgc = 46; break;
	case CONIO_LIGHTRED:       CONIO.bgc = 41; break;
	case CONIO_LIGHTMAGENTA:   CONIO.bgc = 45; break;
	case CONIO_YELLOW:         CONIO.bgc = 43; break;
	case CONIO_WHITE:          CONIO.bgc = 47; break;
	}
	CONIO_apply_attr();
}

inline void CONIO_textcolor(int color)
{
	CONIO.fgc = color % 16;
	CONIO.blink = (color & CONIO_BLINK) != 0;
	CONIO_apply_attr();
}

// newattr packs DOS's text attribute byte: bits 0-3 foreground, bits 4-6
// background, bit 7 blink - the same layout textbackground/textcolor/blink
// already track separately, just combined into one call.
inline void CONIO_textattr(int newattr)
{
	CONIO.fgc = newattr & 0x0F;
	CONIO.blink = (newattr & 0x80) != 0;
	CONIO_textbackground((newattr >> 4) & 0x07);
}

inline void CONIO_highvideo(void)
{
	CONIO.fgc |= 0x08;
	CONIO_apply_attr();
}

inline void CONIO_lowvideo(void)
{
	CONIO.fgc &= ~0x08;
	CONIO_apply_attr();
}

inline void CONIO_normvideo(void)
{
	CONIO.fgc = CONIO_LIGHTGRAY;
	CONIO.bgc = 40;
	CONIO.blink = false;
	CONIO_apply_attr();
}

// Single-slot keyboard pushback, read()-based so it never disagrees with the
// raw termios reads below about which bytes are still waiting on the fd
// (mixing raw reads with buffered stdio getchar()/ungetc() can silently
// swallow bytes stdio has already pulled into its own buffer).
inline int CONIO_ungetbuf = -1;

inline int CONIO_getc(void)
{
	if (CONIO_ungetbuf != -1) {
		int ch = CONIO_ungetbuf;
		CONIO_ungetbuf = -1;
		return ch;
	}
	unsigned char ch;
	return (read(STDIN_FILENO, &ch, 1) == 1) ? ch : EOF;
}

inline int CONIO_ungetch(int ch)
{
	if (CONIO_ungetbuf != -1) {
		return EOF;
	}
	CONIO_ungetbuf = ch;
	return ch;
}

inline int CONIO_getch_echo(bool echo = true)
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~ICANON;
	if (echo) {
		newt.c_lflag |= ECHO;
	} else {
		newt.c_lflag &= ~ECHO;
	}
	// Explicit, not inherited from oldt: on Linux, c_cc[VMIN]/c_cc[VTIME]
	// alias the canonical-mode VEOF/VEOL slots, so leaving them untouched
	// can make a single-byte read block until several bytes are typed.
	newt.c_cc[VMIN] = 1;
	newt.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	// Reading via a raw read() here (see CONIO_getc) bypasses stdio's stdin
	// entirely, which also bypasses the courtesy some libc stdio
	// implementations give interactive programs of flushing stdout before a
	// blocking stdin read on the same terminal. Without this, every prior
	// cprintf/gotoxy/textcolor call - almost none of which end in a newline -
	// stays stuck in stdout's buffer, and the screen looks blank until a key
	// is pressed blind.
	fflush(stdout);
	int ch = CONIO_getc();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

// Arrow keys arrive as a 3-byte ANSI sequence (ESC [ A/B/C/D) instead of DOS's
// single extended-key byte, so a lone getch() can't see the whole thing at
// once. Called only after an ESC has already been read; peeks the next one or
// two bytes with a short timeout (so a real standalone ESC keypress, which
// has nothing following it, still returns immediately) and maps a recognized
// arrow sequence to the matching DOS scancode (72/80/75/77). Anything else is
// pushed back byte-by-byte so it isn't lost, and is treated as a plain ESC.
inline int CONIO_decode_escape(void)
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_cc[VMIN] = 0;
	newt.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	int scancode = -1;
	int b1 = CONIO_getc();
	if (b1 == '[') {
		int b2 = CONIO_getc();
		switch (b2) {
		case 'A': scancode = 72; break;
		case 'B': scancode = 80; break;
		case 'C': scancode = 77; break;
		case 'D': scancode = 75; break;
		default:
			if (b2 != EOF) {
				CONIO_ungetch(b2);
			}
			break;
		}
	} else if (b1 != EOF) {
		CONIO_ungetch(b1);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return scancode;
}

inline int CONIO_scancode = -1;

// Extended keys (currently just the arrows) follow DOS's two-call protocol:
// this call returns 0, and the following getch() call returns the scancode.
// getche() intentionally doesn't decode extended keys - echoing a raw
// scancode byte back to the screen isn't meaningful, and it's a line-input
// function, not a menu-navigation one.
inline int CONIO_getch(void)
{
	if (CONIO_scancode != -1) {
		int scancode = CONIO_scancode;
		CONIO_scancode = -1;
		return scancode;
	}
	int ch = CONIO_getch_echo(false);
	if (ch == '\x1B') {
		int scancode = CONIO_decode_escape();
		if (scancode != -1) {
			CONIO_scancode = scancode;
			return 0;
		}
	}
	return ch;
}

inline int CONIO_getche(void)
{
	return CONIO_getch_echo(true);
}

inline int CONIO_wherexy(int &x, int &y)
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_cc[VMIN] = 0;
	newt.c_cc[VTIME] = 5;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	printf("\033[6n");
	fflush(stdout);

	// Each read below can time out (half a second, so a terminal that never
	// answers the position query - piped/non-interactive input, or one that
	// just doesn't support it - fails fast instead of hanging forever.
	int ok = 0;
	if (CONIO_getc() == '\x1B' && CONIO_getc() == '\x5B') {
		int in;
		int ly = 0;
		while ((in = CONIO_getc()) != ';' && in != EOF) {
			ly = ly * 10 + in - '0';
		}
		int lx = 0;
		while ((in = CONIO_getc()) != 'R' && in != EOF) {
			lx = lx * 10 + in - '0';
		}
		if (in != EOF) {
			x = lx;
			y = ly;
			ok = 1;
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ok;
}

// CONIO_wherexy() above returns absolute screen coordinates; wherex/wherey
// are documented as window-relative, so the offset is undone here. Borland's
// real wherex/wherey can never fail (BIOS always knows the cursor position),
// so there's no documented failure return to mimic; falling back to the
// window's top-left keeps clreol() (built on wherex()) from overrunning the
// window on a terminal that doesn't answer the position query, instead of
// running off with whatever garbage offset a failed query would produce.
inline int CONIO_wherex(void)
{
	int x = 0;
	int y = 0;
	if (!CONIO_wherexy(x, y)) {
		return 1;
	}
	return x - CONIO.wleft + 1;
}

inline int CONIO_wherey(void)
{
	int x = 0;
	int y = 0;
	if (!CONIO_wherexy(x, y)) {
		return 1;
	}
	return y - CONIO.wtop + 1;
}

// Erases from the cursor to the window's right edge, not the physical
// terminal's - needs the cursor's current window-relative column, which
// costs a wherex() round-trip since output through cprintf (a bare printf
// alias) is invisible to this library and can't be tracked internally.
inline void CONIO_clreol(void)
{
	int x = CONIO_wherex();
	int width = (CONIO.wright - CONIO.wleft + 1) - x + 1;
	if (width > 0) {
		printf("\033[%dX", width);
	}
}

inline int CONIO_kbhit(void)
{
	if (CONIO_scancode != -1 || CONIO_ungetbuf != -1) {
		return 1;
	}
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_cc[VMIN] = 1;
	newt.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
	// kbhit() doesn't block, so unlike getch() this can't rely on the read
	// itself eventually forcing output out - a poll loop like
	// "while (!kbhit()) { ... }" would spin with the screen still unflushed.
	fflush(stdout);
	int ch = CONIO_getc();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);
	if (ch != EOF) {
		CONIO_ungetch(ch);
		return 1;
	}
	return 0;
}

inline int CONIO_putch(int c)
{
	printf("%c", c);
	return c;
}

inline int CONIO_cputs(const char *str)
{
	printf("%s", str);
	return 0;
}

// Borland's cgets protocol: str[0] is the caller-supplied max length on entry,
// str[1] is the actual length written back, and the read text starts at str[2]
inline char *CONIO_cgets(char *str)
{
	unsigned char maxlen = (unsigned char)str[0];
	unsigned char count = 0;
	char *text = str + 2;

	while (count < maxlen) {
		int ch = CONIO_getch();
		if (ch == '\r' || ch == '\n') {
			break;
		}
		if (ch == '\b' || ch == 127) {
			if (count > 0) {
				count--;
				printf("\b \b");
			}
			continue;
		}
		text[count++] = (char)ch;
		CONIO_putch(ch);
	}
	text[count] = '\0';
	str[1] = (char)count;

	return text;
}

// Like cgets, but silent - no echo at all, not even a placeholder character,
// matching Borland's real getpass(). Returns a pointer to an internal
// static buffer, same convention as the Unix getpass() this shadows.
inline char CONIO_getpass_buf[128];

inline char *CONIO_getpass(const char *prompt)
{
	printf("%s", prompt);
	fflush(stdout);

	size_t count = 0;
	while (count < sizeof(CONIO_getpass_buf) - 1) {
		int ch = CONIO_getch();
		if (ch == '\r' || ch == '\n') {
			break;
		}
		if (ch == '\b' || ch == 127) {
			if (count > 0) {
				count--;
			}
			continue;
		}
		CONIO_getpass_buf[count++] = (char)ch;
	}
	CONIO_getpass_buf[count] = '\0';
	printf("\n");

	return CONIO_getpass_buf;
}

inline void CONIO_reset(void)
{
	CONIO.fgc = CONIO_LIGHTGRAY;
	CONIO.bgc = 40;
	CONIO.blink = false;
	CONIO.wleft = 1;
	CONIO.wtop = 1;
	CONIO.wright = 80;
	CONIO.wbottom = 25;
	// Full physical clear, not just the DOS-window-bounded one clrscr() would
	// do - a demo run in a terminal bigger than 80x25 shouldn't leave a dirty
	// margin outside the DOS-sized window once it's done. DECSCUSR has no
	// portable way to query a terminal's cursor shape (unlike position via
	// DSR), so a genuine store/restore of whatever shape was active before
	// the program ran isn't possible - but "\033[0 q" is specifically
	// defined as "reset to the terminal's own default", which restores
	// whatever style is configured in the user's terminal profile without
	// needing to know it.
	printf("\033[?25h\033[0 q\033[m\033[2J\033[1;1f");
}

#endif
