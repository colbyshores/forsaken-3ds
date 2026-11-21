// Banner test app: a hello-world CIA that carries the animated HOME-menu
// banner (banner/SPEC.md) under its own title ID, so the banner can be
// tested on hardware without touching the Forsaken install.
#include <3ds.h>
#include <stdio.h>

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	printf("\x1b[12;10HHello, world!\n");
	printf("\x1b[14;4HForsaken banner test (0xff4c0)\n");
	printf("\x1b[29;12HPress START to exit\n");
	while (aptMainLoop())
	{
		hidScanInput();
		if (hidKeysDown() & KEY_START)
			break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	gfxExit();
	return 0;
}
