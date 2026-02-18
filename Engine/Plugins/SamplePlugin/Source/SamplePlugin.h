#pragma once
#include "Runtime/EngineCore/IModule.h"

class SamplePlugin : public IModule
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
    std::string GetModuleName() const override { return "SamplePlugin"; }
    std::string GetModuleVersion() const override { return "1.0.0"; }
};
