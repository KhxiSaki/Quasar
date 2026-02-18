CreationArt Engine
=============

Welcome to the CreationArt Engine source code!

Branches
--------

We publish source for the engine in several branches:

*   Most active development on CAE happens in the **[master](https://github.com/KhxiSaki/CreationArtEngine/tree/master)** branch. This branch reflects the cutting edge of the engine and may be buggy — it may not even compile. We make it available for battle-hardened developers eager to test new features or work in lock-step with us.

    If you choose to work in this branch, be aware that it is likely to be **ahead** of the branches for the current official release and the next upcoming release. Therefore, content and code that you create to work with the ue5-main branch may not be compatible with public releases until we create a new branch directly from ue5-main for a future official release.

Getting up and running
----------------------

The steps below take you through cloning your own private fork, then compiling and running the editor yourself:

### Windows

1.  Install **[GitHub Desktop for Windows](https://desktop.github.com/)** or Install **[Fork - a fast and friendly git client for Mac and Windows](https://git-fork.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 

    Other options:

    -   To use Git from the command line, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

    -   If you'd prefer not to use Git, you can get the source with the **Download ZIP** button on the right. Note that the zip utility built in to Windows marks the contents of .zip files downloaded from the Internet as unsafe to execute, so right-click the .zip file and select **Properties…** and **Unblock** before decompressing it.

1. Install **[Python](https://www.python.org/downloads/)**
    - Download Python 3.14 or later

    - During installation, **make sure to check "Add Python to PATH"**
    
    - Verify installation by opening a command prompt and typing:
        ```
        python --version
        ```
        You should see `Python 3.14.x` or later.

1.  Install **Visual Studio 2022**.

    All desktop editions of Visual Studio 2022, **17.4** or later, **17.8** recommended, can build UE5, including [Visual Studio Community](https://www.visualstudio.com/products/visual-studio-community-vs), which is free for small teams and individual developers.

1.  Open your source folder in Windows Explorer and run **Setup.bat**. This will download install prerequisites and dependencies, and create project files for the engine.

1.  Run **GenerateProjectFiles.bat** to create project files for the engine. It should take less than a minute to complete.

1.  Load the project into Visual Studio by double-clicking the new **CreationArtEngine.sln** file.

1.  Set your solution configuration and your solution platform to **Win64**, then right click the **Engine** target and select **Build**.

1.  After compiling finishes, you can run the editor from Visual Studio by setting your startup project to **Engine** and pressing **F5** to start debugging.

Plugin System (v0.1)
--------------------

The engine uses a UE5-style runtime plugin system. Plugins are dynamically loaded at runtime from `.plugin` descriptor files.

### Plugin Structure

```
Engine/Plugins/ (or Game/Plugins/)
└── YourPlugin/
    ├── YourPlugin.plugin      # JSON descriptor
    ├── Source/
    │   ├── YourPlugin.h
    │   └── YourPlugin.cpp
    └── Binaries/              # Output DLLs (auto-generated)
```

### Creating a Plugin

1. **Create folder** in `Engine/Plugins/` or `Game/Plugins/`

2. **Add descriptor** (`YourPlugin.plugin`):
```json
{
    "Name": "YourPlugin",
    "Version": "1.0.0",
    "Description": "My plugin",
    "Author": "Your Name",
    "EnabledByDefault": true,
    "Dependencies": [],
    "Modules": [
        {
            "Name": "YourPlugin",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

3. **Add source** with module implementation:
```cpp
// YourPlugin.h
#pragma once
#include "Runtime/EngineCore/IModule.h"

class YourPlugin : public IModule
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
    std::string GetModuleName() const override { return "YourPlugin"; }
    std::string GetModuleVersion() const override { return "1.0.0"; }
};
```

```cpp
// YourPlugin.cpp
#include "YourPlugin.h"
#include <iostream>

void YourPlugin::StartupModule()
{
    std::cout << "[YourPlugin] Started!\n";
}

void YourPlugin::ShutdownModule()
{
    std::cout << "[YourPlugin] Shutdown!\n";
}

DECLARE_MODULE(YourPlugin)
```

4. **Add premake** (`premake5.lua`):
```lua
project "YourPlugin"
    kind "SharedLib"
    language "C++"
    cppdialect "C++23"
    staticruntime "off"

    targetdir ("Binaries/" .. outputdir .. "/")
    objdir ("Intermediate/" .. outputdir .. "/")

    files { "Source/**.h", "Source/**.cpp" }

    includedirs { "../../../Engine/Source" }
    links { "Engine" }

    filter "system:windows"
        defines { "CAE_PLATFORM_WINDOWS" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
```

5. **Include in build** - add to root `premake5.lua`:
```lua
group "Plugins"
include "Engine/Plugins/YourPlugin"
group ""
```

6. **Link to Game** - in `Game/premake5.lua`:
```lua
links { "Engine", "YourPlugin" }
dependson { "YourPlugin" }
```

### Using Plugins

```cpp
#include "ModuleManager.h"

// Check if loaded
if (ModuleManager::Get().IsModuleLoaded("YourPlugin"))
{
    // Get the module
    IModule* module = ModuleManager::Get().GetModule("YourPlugin");
}
```

For typed access, create interface classes (e.g., `IRendererPlugin`) that plugins implement.

### Loading Phases

Plugins can specify loading phases in their descriptor:
- `Earliest` - Loaded first
- `PreDefault` - Before default
- `Default` - Normal loading
- `PostDefault` - After default
- `Late` - Loaded last

### Key Files

- `Engine/Source/Runtime/EngineCore/IModule.h` - Module interface
- `Engine/Source/Runtime/EngineCore/PluginDescriptor.h` - Descriptor parsing
- `Engine/Source/Runtime/EngineCore/ModuleManager.h` - Plugin loading management

Licensing
---------

Your access to and use of CreationArt Engine on GitHub is governed by an End User License Agreement (EULA) **Not EULA yet**. If you don't agree to the terms in your chosen EULA, as amended from time to time, you are not permitted to access or use Unreal Engine.

Contributions
-------------

We welcome contributions to CreationArt Engine development through [pull requests](https://github.com/KhxiSaki/CreationArtEngine/pulls/) on GitHub.
