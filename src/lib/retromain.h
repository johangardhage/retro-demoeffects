//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMAIN_H_
#define _RETROMAIN_H_

#include "retro.h"

void RETRO_ParseArguments(int argc, char *argv[])
{
	RETRO.basename = basename(argv[0]);
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"window", no_argument, 0, 'w'},
		{"fullwindow", no_argument, 0, 0},
		{"fullscreen", no_argument, 0, 'f'},
		{"stretch", no_argument, 0, 's'},
		{"nostretch", no_argument, 0, 0},
		{"vsync", no_argument, 0, 'v'},
		{"novsync", no_argument, 0, 0},
		{"linear", no_argument, 0, 'l'},
		{"nolinear", no_argument, 0, 0},
		{"showcursor", no_argument, 0, 'c'},
		{"nocursor", no_argument, 0, 0},
		{"showfps", no_argument, 0, 0},
		{"nofps", no_argument, 0, 0},
		{"capfps", required_argument, 0, 0},
		{0, 0, 0, 0} };
	bool usage = false;
	int c;
	int option_index = 0;
	while ((c = getopt_long(argc, argv, ":hwfslcv", long_options, &option_index)) != -1) {
		switch (c) {
		case 0:
			if (strcmp("fullwindow", long_options[option_index].name) == 0) {
				RETRO.mode = RETRO_MODE_FULLWINDOW;
			} else if (strcmp("nostretch", long_options[option_index].name) == 0) {
				RETRO.stretch = false;
			} else if (strcmp("novsync", long_options[option_index].name) == 0) {
				RETRO.vsync = false;
			} else if (strcmp("nolinear", long_options[option_index].name) == 0) {
				RETRO.linear = false;
			} else if (strcmp("nocursor", long_options[option_index].name) == 0) {
				RETRO.showcursor = false;
			} else if (strcmp("showfps", long_options[option_index].name) == 0) {
				RETRO.showfps = true;
			} else if (strcmp("nofps", long_options[option_index].name) == 0) {
				RETRO.showfps = false;
			} else if (strcmp("capfps", long_options[option_index].name) == 0) {
				RETRO.fpscap = atoi(optarg);
			}
			break;
		case 'h':
			usage = true;
			break;
		case 'w':
			RETRO.mode = RETRO_MODE_WINDOW;
			break;
		case 'f':
			RETRO.mode = RETRO_MODE_FULLSCREEN;
			break;
		case 's':
			RETRO.stretch = true;
			break;
		case 'v':
			RETRO.vsync = true;
			break;
		case 'l':
			RETRO.linear = true;
			break;
		case 'c':
			RETRO.showcursor = true;
			break;
		case '?':
			usage = true;
			printf("unrecognized option '%s'\n", argv[optind - 1]);
			break;
		default:
			usage = true;
			break;
		}
	}
	if (optind < argc) {
		usage = true;
		printf("non-option ARGV-elements: ");
		while (optind < argc) {
			printf("%s ", argv[optind++]);
		}
		printf("\n");
	}
	if (usage) {
		printf("Usage: %s [OPTION]...\n\n", RETRO.basename);
		printf("Options:\n");
		printf(" -h, --help           Display this text and exit\n");
		printf(" -w, --window         Render in a window\n");
		printf("     --fullwindow     Render in a fullscreen window\n");
		printf(" -f, --fullscreen     Render in fullscreen\n");
		printf(" -s, --stretch        Stretch the screen\n");
		printf("     --nostretch      Use fixed resolution for rendering\n");
		printf(" -v, --vsync          Enable sync to vertical refresh\n");
		printf("     --novsync        Disable sync to vertical refresh\n");
		printf(" -l, --linear         Render using linear filtering\n");
		printf("     --nolinear       Render using nearest pixel sampling\n");
		printf(" -c, --showcursor     Show mouse cursor\n");
		printf("     --nocursor       Hide mouse cursor\n");
		printf("     --showfps        Show frame rate in window title\n");
		printf("     --nofps          Hide frame rate\n");
		printf("     --capfps=VALUE   Limit frame rate to the specified VALUE\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	RETRO_ParseArguments(argc, argv);

	if (DEMO_Startup) DEMO_Startup();

	if (DEMO_Initialize || DEMO_Deinitialize || DEMO_Render || DEMO_Render2 ) {
		RETRO_Initialize();
		if (DEMO_Initialize) DEMO_Initialize();

		if (DEMO_Render || DEMO_Render2) RETRO_Mainloop();

		if (DEMO_Deinitialize) DEMO_Deinitialize();
		RETRO_Deinitialize();
	}

	return 0;
}

#endif
