#pragma once
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

enum class ELoadingPhase
{
    Earliest,
    PreDefault,
    Default,
    PostDefault,
    Late,
    NumPhases
};

enum class EModuleType
{
    Runtime,
    Editor,
    Developer,
    Program,
    Unknown
};

struct ModuleDescriptor
{
    std::string Name;
    EModuleType Type = EModuleType::Runtime;
    ELoadingPhase LoadingPhase = ELoadingPhase::Default;
};

struct PluginDescriptor
{
    std::string Name;
    std::string Version = "1.0.0";
    std::string Description;
    std::string Author;
    std::vector<ModuleDescriptor> Modules;
    std::vector<std::string> Dependencies;
    bool bEnabledByDefault = true;
    bool bIsBeta = false;

    static std::optional<PluginDescriptor> LoadFromFile(const std::filesystem::path& DescriptorPath);
    bool SaveToFile(const std::filesystem::path& DescriptorPath) const;
};
