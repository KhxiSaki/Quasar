#include "Runtime/EngineCore/Application.h"

#ifdef CAE_PLATFORM_WINDOWS

extern Application* CreateApplication(int argc, char** argv);

int Main(int argc, char** argv)
{
	Application* app = CreateApplication(argc, argv);
	app->Run();
	delete app;
	return 0;
}

#ifdef CAE_DIST

#include <Windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return Main(argc, argv);
}

#endif // CAE_DIST

#else

// For non-Windows platforms
extern Application* CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	Application* app = CreateApplication(argc, argv);
	app->Run();
	delete app;
	return 0;
}

#endif // CAE_PLATFORM_WINDOWS
