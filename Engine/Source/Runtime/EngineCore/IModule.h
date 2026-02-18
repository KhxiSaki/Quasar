#pragma once
#include <string>

class IModule
{
public:
    virtual ~IModule() = default;

    virtual void StartupModule() = 0;
    virtual void ShutdownModule() = 0;

    virtual std::string GetModuleName() const = 0;
    virtual std::string GetModuleVersion() const = 0;
};

#define DECLARE_MODULE(ModuleClass) \
    extern "C" __declspec(dllexport) IModule* CreateModule() \
    { \
        return new ModuleClass(); \
    }
