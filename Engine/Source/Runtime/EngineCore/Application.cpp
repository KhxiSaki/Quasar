#include "Runtime/EngineCore/Application.h"

Application::Application()
{
    std::cout << "Initializing application..." << std::endl;
    m_GameEngine = std::make_unique<GameEngine>();
}

Application::~Application()
{
}

void Application::Run()
{
    m_GameEngine->Run();
}