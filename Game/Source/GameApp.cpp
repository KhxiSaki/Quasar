#include "Runtime/EngineCore/Application.h"

class GameApp : public Application
{
public:
	GameApp()
	{
		// Initialize your game here
	}

	virtual ~GameApp()
	{
		// Cleanup your game here
	}

protected:
	// Override any Applications methods if needed
	void Run() override
	{
		// Your game run logic here
		Application::Run();
	}
};

Application* CreateApplication(int argc, char** argv)
{
	return new GameApp();
}