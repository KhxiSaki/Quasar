#include "Runtime/EngineCore/GameEngine.h"

GameEngine::GameEngine()
{
    Initialize();
}

GameEngine::~GameEngine()
{
    Cleanup();
}

void GameEngine::Run()
{
    MainLoop();
    Cleanup();
}

void GameEngine::Initialize()
{
    m_Window = new Window("CreationArtEngine", 800, 600);
    m_Renderer = std::make_unique<Renderer>(m_Window);

    std::cout << "Initializing GameEngine..." << std::endl;
    m_Renderer->Initialize();
}

void GameEngine::MainLoop()
{
    while (bIsRunning && !m_Window->closed()) {
        m_Window->Update();
        
        OnUpdate(m_Window->deltaTime);
        
        m_Renderer->Render();
        
        OnRender();
    }
    m_Renderer->GetDevice().waitIdle();
}

void GameEngine::Shutdown()
{
    bIsRunning = false;
}

void GameEngine::Cleanup()
{
    m_Renderer->Shutdown();
    delete m_Window;
    m_Window = nullptr;
}
