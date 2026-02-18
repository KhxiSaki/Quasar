#include "ModuleManager.h"
#include <iostream>
#include <algorithm>

#ifdef CAE_PLATFORM_WINDOWS
#define PLATFORM_DLOPEN(path) LoadLibraryW(path.c_str())
#define PLATFORM_DLSYM(handle, name) GetProcAddress(handle, name)
#define PLATFORM_DLCLOSE(handle) FreeLibrary(handle)
#define PLATFORM_GET_ERROR() std::system_category().message(GetLastError())
#else
#define PLATFORM_DLOPEN(path) dlopen(path.c_str(), RTLD_NOW)
#define PLATFORM_DLSYM(handle, name) dlsym(handle, name)
#define PLATFORM_DLCLOSE(handle) dlclose(handle)
#define PLATFORM_GET_ERROR() dlerror()
#endif

std::filesystem::path ModuleManager::GetPluginBinaryPath(const std::string& PluginName, const std::filesystem::path& PluginDir)
{
#ifdef CAE_PLATFORM_WINDOWS
    return PluginDir / "Binaries" / (PluginName + ".dll");
#elif __APPLE__
    return PluginDir / "Binaries" / ("lib" + PluginName + ".dylib");
#else
    return PluginDir / "Binaries" / ("lib" + PluginName + ".so");
#endif
}

void ModuleManager::LoadPluginsFromDirectory(const std::filesystem::path& PluginDir)
{
    if (!std::filesystem::exists(PluginDir))
    {
        std::cerr << "[ModuleManager] Plugin directory does not exist: " << PluginDir << "\n";
        return;
    }

    std::cout << "[ModuleManager] Scanning plugin directory: " << PluginDir << "\n";
    PluginSearchPaths.push_back(PluginDir);

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(PluginDir))
    {
        if (Entry.path().filename().string().starts_with("."))
            continue;

        if (Entry.path().extension() == ".plugin")
        {
            std::cout << "[ModuleManager] Found plugin descriptor: " << Entry.path() << "\n";
            auto descriptor = PluginDescriptor::LoadFromFile(Entry.path());
            if (descriptor)
            {
                std::cout << "[ModuleManager] Parsed plugin: " << descriptor->Name 
                          << " (Enabled: " << descriptor->bEnabledByDefault << ")\n";
                if (descriptor->bEnabledByDefault)
                {
                    std::cout << "[ModuleManager] Loading plugin: " << descriptor->Name << "\n";
                    LoadPlugin(descriptor->Name);
                }
            }
        }
    }
}

void ModuleManager::LoadPluginsByPhase(const std::filesystem::path& PluginDir, ELoadingPhase Phase)
{
    if (!std::filesystem::exists(PluginDir))
        return;

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(PluginDir))
    {
        if (Entry.path().extension() != ".plugin")
            continue;

        auto descriptor = PluginDescriptor::LoadFromFile(Entry.path());
        if (!descriptor || !descriptor->bEnabledByDefault)
            continue;

        for (const auto& module : descriptor->Modules)
        {
            if (module.LoadingPhase == Phase && !IsModuleLoaded(module.Name))
            {
                auto binaryPath = GetPluginBinaryPath(descriptor->Name, Entry.path().parent_path());
                if (std::filesystem::exists(binaryPath))
                {
                    if (ResolveDependencies(*descriptor, PluginDir))
                    {
                        LoadDynamicModule(module.Name, binaryPath, *descriptor);
                    }
                }
            }
        }
    }
}

bool ModuleManager::ResolveDependencies(const PluginDescriptor& Descriptor, const std::filesystem::path& PluginDir)
{
    for (const auto& dep : Descriptor.Dependencies)
    {
        if (!IsModuleLoaded(dep))
        {
            if (!LoadPlugin(dep))
            {
                std::cerr << "[ModuleManager] Failed to resolve dependency: " << dep << " for " << Descriptor.Name << "\n";
                return false;
            }
        }
    }
    return true;
}

bool ModuleManager::LoadPlugin(const std::string& PluginName)
{
    if (IsModuleLoaded(PluginName))
        return true;

    for (const auto& searchPath : PluginSearchPaths)
    {
        auto pluginPath = searchPath / PluginName;
        auto descriptorPath = pluginPath / (PluginName + ".plugin");

        std::cout << "[ModuleManager] Searching in: " << pluginPath << "\n";

        if (std::filesystem::exists(descriptorPath))
        {
            auto descriptor = PluginDescriptor::LoadFromFile(descriptorPath);
            if (!descriptor)
                continue;

            if (!ResolveDependencies(*descriptor, searchPath))
                return false;

            auto binaryPath = GetPluginBinaryPath(PluginName, pluginPath);
            
            std::string dllName = std::string(PluginName) + ".dll";
            
            // Search in common locations
            std::vector<std::filesystem::path> searchLocations;
            searchLocations.push_back(binaryPath);
            searchLocations.push_back(searchPath / "Binaries" / dllName);
            
            // Recursively search in Binaries folders
            for (const auto& basePath : {"Binaries", ".."})
            {
                if (std::filesystem::exists(basePath))
                {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath))
                    {
                        if (entry.is_regular_file() && entry.path().filename() == dllName)
                        {
                            std::cout << "[ModuleManager] Found plugin at: " << entry.path() << "\n";
                            return LoadDynamicModule(PluginName, entry.path(), *descriptor);
                        }
                    }
                }
            }
            
            for (const auto& path : searchLocations)
            {
                if (std::filesystem::exists(path))
                {
                    return LoadDynamicModule(PluginName, path, *descriptor);
                }
            }
            
            std::cerr << "[ModuleManager] Plugin binary not found for: " << PluginName << "\n";
            return false;
        }
    }

    std::cerr << "[ModuleManager] Plugin descriptor not found: " << PluginName << "\n";
    return false;
}

bool ModuleManager::LoadModule(const std::string& ModuleName)
{
    return LoadPlugin(ModuleName);
}

bool ModuleManager::LoadDynamicModule(const std::string& Name, const std::filesystem::path& DllPath, const PluginDescriptor& Descriptor)
{
    LoadedModule Module;
    Module.Descriptor = Descriptor;

#ifdef CAE_PLATFORM_WINDOWS
    Module.LibraryHandle = PLATFORM_DLOPEN(DllPath.wstring());
#else
    Module.LibraryHandle = PLATFORM_DLOPEN(DllPath.string());
#endif

    if (!Module.LibraryHandle)
    {
        std::cerr << "[ModuleManager] Failed to load library: " << DllPath << "\n";
        std::cerr << "[ModuleManager] Error: " << PLATFORM_GET_ERROR() << "\n";
        return false;
    }

    auto Factory = reinterpret_cast<ModuleFactoryFn>(PLATFORM_DLSYM(Module.LibraryHandle, "CreateModule"));

    if (!Factory)
    {
        std::cerr << "[ModuleManager] CreateModule not found in: " << Name << "\n";
        PLATFORM_DLCLOSE(Module.LibraryHandle);
        return false;
    }

    Module.Instance.reset(Factory());
    if (!Module.Instance)
    {
        std::cerr << "[ModuleManager] Failed to create module instance: " << Name << "\n";
        PLATFORM_DLCLOSE(Module.LibraryHandle);
        return false;
    }

    Module.bIsLoaded = true;
    Module.Instance->StartupModule();

    std::cout << "[ModuleManager] Loaded module: " << Name << " (Version: " << Module.Instance->GetModuleVersion() << ")\n";

    LoadedPlugins.push_back(Descriptor);
    Modules[Name] = std::move(Module);

    return true;
}

void ModuleManager::UnloadModule(const std::string& ModuleName)
{
    auto it = Modules.find(ModuleName);
    if (it == Modules.end())
        return;

    if (it->second.bIsLoaded)
        it->second.Instance->ShutdownModule();

    it->second.Instance.reset();

    if (it->second.LibraryHandle)
        PLATFORM_DLCLOSE(it->second.LibraryHandle);

    LoadedPlugins.erase(
        std::remove_if(LoadedPlugins.begin(), LoadedPlugins.end(),
            [&ModuleName](const PluginDescriptor& desc) { return desc.Name == ModuleName; }),
        LoadedPlugins.end());

    Modules.erase(it);

    std::cout << "[ModuleManager] Unloaded module: " << ModuleName << "\n";
}

void ModuleManager::UnloadAllModules()
{
    for (auto& [Name, Module] : Modules)
    {
        if (Module.bIsLoaded && Module.Instance)
        {
            Module.Instance->ShutdownModule();
            std::cout << "[ModuleManager] Shutdown module: " << Name << "\n";
        }
        if (Module.LibraryHandle)
            PLATFORM_DLCLOSE(Module.LibraryHandle);
    }
    Modules.clear();
    LoadedPlugins.clear();
}

bool ModuleManager::IsModuleLoaded(const std::string& ModuleName) const
{
    auto it = Modules.find(ModuleName);
    return it != Modules.end() && it->second.bIsLoaded;
}
