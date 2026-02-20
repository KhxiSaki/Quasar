#pragma once

#include <memory>
#include "GameEngine.h"

class Application
{
public:
	Application();
	virtual ~Application();

    virtual void Run();

protected:
    std::unique_ptr<GameEngine> m_GameEngine;
};
