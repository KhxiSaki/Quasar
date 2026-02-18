#pragma once
#include "IModule.h"
#include "PluginDescriptor.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>
#include <vector>

#ifdef CAE_PLATFORM_WINDOWS
#include <Windows.h>
using LibHandle = HMODULE;
#else
#include <dlfcn.h>
using LibHandle = void*;
#endif

using ModuleFactoryFn = IModule*(*)();

struct LoadedModule
{
    std::unique_ptr<IModule> Instance;
    LibHandle                 LibraryHandle = nullptr;
    bool                      bIsLoaded = false;
    PluginDescriptor          Descriptor;
};

class ModuleManager
{
public:
    static ModuleManager& Get()
    {
        static ModuleManager Instance;
        return Instance;
    }

    void LoadPluginsFromDirectory(const std::filesystem::path& PluginDir);
    void LoadPluginsByPhase(const std::filesystem::path& PluginDir, ELoadingPhase Phase);
    
    bool LoadModule(const std::string& ModuleName);
    bool LoadPlugin(const std::string& PluginName);
    void UnloadModule(const std::string& ModuleName);
    void UnloadAllModules();

    template<typename T>
    T* GetModule(const std::string& ModuleName)
    {
        auto it = Modules.find(ModuleName);
        if (it != Modules.end() && it->second.bIsLoaded)
            return static_cast<T*>(it->second.Instance.get());
        return nullptr;
    }

    IModule* GetModule(const std::string& ModuleName)
    {
        auto it = Modules.find(ModuleName);
        if (it != Modules.end() && it->second.bIsLoaded)
            return it->second.Instance.get();
        return nullptr;
    }

    bool IsModuleLoaded(const std::string& ModuleName) const;
    const std::vector<PluginDescriptor>& GetLoadedPlugins() const { return LoadedPlugins; }

private:
    ModuleManager() = default;
    ~ModuleManager() { UnloadAllModules(); }

    bool LoadDynamicModule(const std::string& Name, const std::filesystem::path& DllPath, const PluginDescriptor& Descriptor);
    bool ResolveDependencies(const PluginDescriptor& Descriptor, const std::filesystem::path& PluginDir);
    std::filesystem::path GetPluginBinaryPath(const std::string& PluginName, const std::filesystem::path& PluginDir);

    std::unordered_map<std::string, LoadedModule> Modules;
    std::vector<PluginDescriptor> LoadedPlugins;
    std::vector<std::filesystem::path> PluginSearchPaths;
};
