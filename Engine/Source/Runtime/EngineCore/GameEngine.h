#pragma once

#include <memory>
#include "Window.h"
#include "Renderer/Renderer.h"

struct GameEngine
{
public:
    GameEngine();
    ~GameEngine();

    void Run();
    void Shutdown();

    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}

    bool IsRunning() const { return bIsRunning; }
    void Stop() { bIsRunning = false; }

    Window* GetWindow() const { return m_Window; }
    Renderer* GetRenderer() const { return m_Renderer.get(); }

private:
    void Initialize();
    void MainLoop();
    void Cleanup();

    bool bIsRunning = true;
    Window* m_Window = nullptr;
    std::unique_ptr<Renderer> m_Renderer;
};
