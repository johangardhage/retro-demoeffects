//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMAIN_H_
#define _RETROMAIN_H_

#include "retro.h"

int main(int argc, char *argv[])
{
	if (DEMO_Startup != NULL) DEMO_Startup();

	RETRO_Initialize(argc, argv, RETRO_FULLSCREEN | RETRO_VSYNC | RETRO_SHOWFPS);
	RETRO_Mainloop();
	RETRO_Deinitialize();

	return 0;
}

#endif
