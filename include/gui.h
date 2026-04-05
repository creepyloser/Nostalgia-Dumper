#pragma once

#include "types.h"
#include <map>
#include <string>
#include <thread>
#include <vector>
#include <imgui.h>

namespace GUI
{
    struct RecentDump
    {
        std::string gameName;
        std::string engine;
        std::string date;
        int foundCount;
    };

    struct AppState
    {
        // Window state (Custom Topbar)
        bool isDragging = false;
        ImVec2 dragOffset;
        bool shouldClose = false;
        bool shouldMinimize = false;

        // Process selection
        std::string selectedProcessName;
        u32 selectedProcessId = 0;
        std::vector<ProcessInfo> availableProcesses;
        char processSearchBuffer[256] = { 0 };
        int selectedProcessIndex = -1;
        
        // Engine: 0 Unreal, 1 Unity IL2CPP, 2 Unity Mono, 3 Source, 4 Source 2, 5 Godot, 6 CryEngine, 7 Universal
        int selectedEngine = 0;
        bool deepUeScan = false;

        // Scanning state
        bool isScanning = false;
        bool scanComplete = false;
        bool showScanCompletePopup = false;
        DiscoveredOffsets foundOffsets;
        std::map<std::string, u64> engineSymbols;
        std::string scanLog;
        float scanProgress = 0.0f;
        std::thread* scanThread = nullptr;
        
        // Output
        std::string outputDirectory;
        char outputPathBuffer[512] = { 0 };
        std::string generatedHeaderContent;
        bool headerGenerated = false;
        
        // UI state
        bool showAbout = false;
        bool darkTheme = true;
        bool autoRefresh = true;
        float lastRefreshTime = 0.0f;
        int contentTab = 0;
        char globalFilter[256] = { 0 };
        char layoutFilter[256] = { 0 };

        // Settings
        bool showChangelog = false;
        bool changelogShown = false;
        float uiOpacity = 1.0f;
        bool autoExport = false;
        bool multiThreadedScan = true;
        bool heuristicFallback = true;

        // Recents
        std::vector<RecentDump> recentDumps;

        // Custom Scan / Pattern Finder
        char customPattern[512] = { 0 };
        char customModule[256] = { 0 };
        int customOffset = 0;
        bool customScanRunning = false;
        std::string customScanResult;
        char stringSearchBuffer[256] = { 0 };
        std::vector<u64> stringSearchResults;
        
        // Error dialog
        bool showErrorDialog = false;
        std::string errorTitle;
        std::string errorMessage;
    };
    
    bool Initialize();
    void Shutdown();
    bool RenderFrame(AppState& appState);
    void RefreshProcessList(AppState& appState);
    void RequestScan(AppState& appState);
    void RequestHeaderGeneration(AppState& appState);
    void ShowError(AppState& appState, const std::string& title, const std::string& message);
    void SetDarkTheme();
    void GetOptimalWindowSize(int& outWidth, int& outHeight);
    void RequestExit();

    // UI Helpers
    void TextCentered(const char* text);
}

namespace ImGui {
    void VerticalSpacing(float size);
}
