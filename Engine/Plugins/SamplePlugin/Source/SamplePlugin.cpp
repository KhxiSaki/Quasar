#include "SamplePlugin.h"
#include <iostream>

void SamplePlugin::StartupModule()
{
    std::cout << "[SamplePlugin] Started!\n";
}

void SamplePlugin::ShutdownModule()
{
    std::cout << "[SamplePlugin] Shutdown!\n";
}

DECLARE_MODULE(SamplePlugin)
