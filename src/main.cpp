#include "gui.h"
#include "version.h"
#include <windows.h>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[])
{
    // Initialize GUI subsystem
    if (!GUI::Initialize())
    {
        std::cerr << "Error: Failed to initialize GUI" << std::endl;
        return 1;
    }

    GUI::AppState appState;
    GUI::SetDarkTheme();
    
    // Initial process list refresh
    GUI::RefreshProcessList(appState);

    // Main application loop
    while (GUI::RenderFrame(appState))
    {
        // RenderFrame handles the window message pump and ImGui rendering
    }

    // Cleanup and exit
    if (appState.scanThread && appState.scanThread->joinable())
    {
        appState.scanThread->join();
        delete appState.scanThread;
    }

    GUI::Shutdown();
    return 0;
}
