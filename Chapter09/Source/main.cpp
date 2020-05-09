#include "Application.h"
#include "utils.h"

#ifdef _DEBUG
int main()
#else
int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
#endif
{
	DebugOutputFromString("toon shading test.");

	Application app;
	app.Initialize();
	app.Run();
	app.Terminate();

	return 0;
}

