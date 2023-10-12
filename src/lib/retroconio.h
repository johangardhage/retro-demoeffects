//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCONIO_H_
#define _RETROCONIO_H_

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define cprintf printf
#define cscanf scanf
#define cgets gets

#define gotoxy          CONIO_gotoxy
#define clrscr          CONIO_clrscr
#define textcolor       CONIO_textcolor
#define textbackground  CONIO_textbackground
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
#define gettext         CONIO_gettext
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

struct {
	int bgc = 40;
} CONIO;

void CONIO_clreol(void)
{
	printf("\033[K");
}

void CONIO_insline(void)
{
	printf("\x1b[1L");
}

void CONIO_delline(void)
{
	printf("\033[1M");
}

void CONIO_gotoxy(int x, int y)
{
	printf("\033[%d;%df", y, x);
}

void CONIO_clrscr(void)
{
	printf("\033[%dm\033[2J\033[1;1f", CONIO.bgc);
}

void CONIO_textbackground(int color)
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
}

void CONIO_textcolor(short color)
{
	switch (color % 16) {
	case CONIO_BLACK:          printf("\033[0;%d;%dm", 30, CONIO.bgc); break;
	case CONIO_BLUE:           printf("\033[0;%d;%dm", 34, CONIO.bgc); break;
	case CONIO_GREEN:          printf("\033[0;%d;%dm", 32, CONIO.bgc); break;
	case CONIO_CYAN:           printf("\033[0;%d;%dm", 36, CONIO.bgc); break;
	case CONIO_RED:            printf("\033[0;%d;%dm", 31, CONIO.bgc); break;
	case CONIO_MAGENTA:        printf("\033[0;%d;%dm", 35, CONIO.bgc); break;
	case CONIO_BROWN:          printf("\033[0;%d;%dm", 33, CONIO.bgc); break;
	case CONIO_LIGHTGRAY:      printf("\033[0;%d;%dm", 37, CONIO.bgc); break;
	case CONIO_DARKGRAY:       printf("\033[1;%d;%dm", 30, CONIO.bgc); break;
	case CONIO_LIGHTBLUE:      printf("\033[1;%d;%dm", 34, CONIO.bgc); break;
	case CONIO_LIGHTGREEN:     printf("\033[1;%d;%dm", 32, CONIO.bgc); break;
	case CONIO_LIGHTCYAN:      printf("\033[1;%d;%dm", 36, CONIO.bgc); break;
	case CONIO_LIGHTRED:       printf("\033[1;%d;%dm", 31, CONIO.bgc); break;
	case CONIO_LIGHTMAGENTA:   printf("\033[1;%d;%dm", 35, CONIO.bgc); break;
	case CONIO_YELLOW:         printf("\033[1;%d;%dm", 33, CONIO.bgc); break;
	case CONIO_WHITE:          printf("\033[1;%d;%dm", 37, CONIO.bgc); break;
	}
}

int CONIO_ungetch(int ch)
{
	return ungetc(ch, stdin);
}

int CONIO_getch_echo(bool echo = true)
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~ICANON;
	if (echo) {
		newt.c_lflag &= ECHO;
	} else {
		newt.c_lflag &= ~ECHO;
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	int ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

int CONIO_getch(void)
{
	return CONIO_getch_echo(false);
}

int CONIO_getche(void)
{
	return CONIO_getch_echo(true);
}

int CONIO_wherexy(int &x, int &y)
{
	printf("\033[6n");
	if (getch() != '\x1B') {
		return 0;
	}
	if (getch() != '\x5B') {
		return 0;
	}
	int in;
	int ly = 0;
	while ((in = getch()) != ';') {
		ly = ly * 10 + in - '0';
	}
	int lx = 0;
	while ((in = getch()) != 'R') {
		lx = lx * 10 + in - '0';
	}
	x = lx;
	y = ly;
	return 0;
}

int CONIO_wherex(void)
{
	int x = 0;
	int y = 0;
	CONIO_wherexy(x, y);
	return x;
}

int CONIO_wherey(void)
{
	int x = 0;
	int y = 0;
	CONIO_wherexy(x, y);
	return y;
}

int CONIO_kbhit(void)
{
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
	int ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);
	if (ch != EOF) {
		ungetc(ch, stdin);
		return 1;
	}
	return 0;
}

int CONIO_putch(const char c)
{
	printf("%c", c);
	return (int)c;
}

int CONIO_cputs(const char *str)
{
	printf(str);
	return 0;
}

int CONIO_gettext(int l, int t, int r, int b, void *destination)
{
	return 0;
}

void CONIO_reset(void)
{
	CONIO.bgc = 40;
	printf("\033[m");
}

#endif
