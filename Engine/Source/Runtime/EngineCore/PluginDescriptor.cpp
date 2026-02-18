#include "PluginDescriptor.h"
#include <fstream>
#include <sstream>

namespace
{
    std::string Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    std::string TrimQuotes(const std::string& str)
    {
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
            return str.substr(1, str.size() - 2);
        return str;
    }

    ELoadingPhase ParseLoadingPhase(const std::string& phase)
    {
        if (phase == "Earliest") return ELoadingPhase::Earliest;
        if (phase == "PreDefault") return ELoadingPhase::PreDefault;
        if (phase == "PostDefault") return ELoadingPhase::PostDefault;
        if (phase == "Late") return ELoadingPhase::Late;
        return ELoadingPhase::Default;
    }

    EModuleType ParseModuleType(const std::string& type)
    {
        if (type == "Runtime") return EModuleType::Runtime;
        if (type == "Editor") return EModuleType::Editor;
        if (type == "Developer") return EModuleType::Developer;
        if (type == "Program") return EModuleType::Program;
        return EModuleType::Unknown;
    }
}

std::optional<PluginDescriptor> PluginDescriptor::LoadFromFile(const std::filesystem::path& DescriptorPath)
{
    std::ifstream file(DescriptorPath);
    if (!file.is_open())
        return std::nullopt;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    PluginDescriptor descriptor;
    descriptor.Name = DescriptorPath.stem().string();

    std::istringstream stream(content);
    std::string line;
    bool inModules = false;
    ModuleDescriptor* currentModule = nullptr;

    while (std::getline(stream, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '/' || line[0] == '{' || line[0] == '}')
            continue;

        if (line == "\"Modules\"")
        {
            inModules = true;
            continue;
        }

        if (inModules)
        {
            if (line.find("[") != std::string::npos)
                continue;
            if (line == "}," || line == "]")
            {
                inModules = false;
                continue;
            }
            if (line.find("\"Name\"") != std::string::npos)
            {
                size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    std::string value = Trim(line.substr(colon + 1));
                    if (currentModule)
                        currentModule->Name = TrimQuotes(value);
                    else
                        descriptor.Name = TrimQuotes(value);
                }
            }
            else if (line.find("\"Type\"") != std::string::npos)
            {
                size_t colon = line.find(':');
                if (colon != std::string::npos && currentModule)
                {
                    std::string value = Trim(line.substr(colon + 1));
                    currentModule->Type = ParseModuleType(TrimQuotes(value));
                }
            }
            else if (line.find("\"LoadingPhase\"") != std::string::npos)
            {
                size_t colon = line.find(':');
                if (colon != std::string::npos && currentModule)
                {
                    std::string value = Trim(line.substr(colon + 1));
                    currentModule->LoadingPhase = ParseLoadingPhase(TrimQuotes(value));
                }
            }
            else if (line == "{")
            {
                descriptor.Modules.push_back(ModuleDescriptor());
                currentModule = &descriptor.Modules.back();
            }
        }
        else
        {
            size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;

            std::string key = Trim(line.substr(0, colon));
            std::string value = Trim(line.substr(colon + 1));

            if (key == "Version")
                descriptor.Version = TrimQuotes(value);
            else if (key == "Description")
                descriptor.Description = TrimQuotes(value);
            else if (key == "Author")
                descriptor.Author = TrimQuotes(value);
            else if (key == "EnabledByDefault")
                descriptor.bEnabledByDefault = (value == "true");
            else if (key == "IsBeta")
                descriptor.bIsBeta = (value == "true");
        }
    }

    if (descriptor.Modules.empty())
    {
        ModuleDescriptor defaultModule;
        defaultModule.Name = descriptor.Name;
        defaultModule.Type = EModuleType::Runtime;
        defaultModule.LoadingPhase = ELoadingPhase::Default;
        descriptor.Modules.push_back(defaultModule);
    }

    return descriptor;
}

bool PluginDescriptor::SaveToFile(const std::filesystem::path& DescriptorPath) const
{
    std::ofstream file(DescriptorPath);
    if (!file.is_open())
        return false;

    file << "{\n";
    file << "    \"Name\": \"" << Name << "\",\n";
    file << "    \"Version\": \"" << Version << "\",\n";
    file << "    \"Description\": \"" << Description << "\",\n";
    file << "    \"Author\": \"" << Author << "\",\n";
    file << "    \"EnabledByDefault\": " << (bEnabledByDefault ? "true" : "false") << ",\n";
    file << "    \"IsBeta\": " << (bIsBeta ? "true" : "false") << ",\n";

    if (!Dependencies.empty())
    {
        file << "    \"Dependencies\": [";
        for (size_t i = 0; i < Dependencies.size(); i++)
        {
            file << "\"" << Dependencies[i] << "\"";
            if (i < Dependencies.size() - 1) file << ", ";
        }
        file << "],\n";
    }

    file << "    \"Modules\": [\n";
    for (size_t i = 0; i < Modules.size(); i++)
    {
        const ModuleDescriptor& mod = Modules[i];
        file << "        {\n";
        file << "            \"Name\": \"" << mod.Name << "\",\n";
        file << "            \"Type\": \"";
        switch (mod.Type)
        {
            case EModuleType::Runtime: file << "Runtime"; break;
            case EModuleType::Editor: file << "Editor"; break;
            case EModuleType::Developer: file << "Developer"; break;
            case EModuleType::Program: file << "Program"; break;
            default: file << "Runtime"; break;
        }
        file << "\",\n";
        file << "            \"LoadingPhase\": \"";
        switch (mod.LoadingPhase)
        {
            case ELoadingPhase::Earliest: file << "Earliest"; break;
            case ELoadingPhase::PreDefault: file << "PreDefault"; break;
            case ELoadingPhase::PostDefault: file << "PostDefault"; break;
            case ELoadingPhase::Late: file << "Late"; break;
            default: file << "Default"; break;
        }
        file << "\"\n";
        file << "        }";
        if (i < Modules.size() - 1) file << ",";
        file << "\n";
    }
    file << "    ]\n";
    file << "}\n";

    return true;
}
