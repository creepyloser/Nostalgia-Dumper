#include "gui.h"
#include "dumper.h"
#include "engine_scans.h"
#include "memory.h"
#include "patterns.h"
#include "version.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <windows.h>
#include <d3d11.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    IDXGISwapChain* g_pSwapChain = nullptr;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
    UINT g_resizeWidth = 0;
    UINT g_resizeHeight = 0;
    HWND g_hWnd = nullptr;
    WNDCLASSEXW g_wc = {};
    bool g_windowClassRegistered = false;

    void CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    void CleanupRenderTarget()
    {
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
    }

    bool CreateDeviceD3D(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel = {};
        const D3D_FEATURE_LEVEL featureLevelArray[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (res == DXGI_ERROR_UNSUPPORTED)
            res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
                featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (res != S_OK)
            return false;
        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D()
    {
        CleanupRenderTarget();
        if (g_pSwapChain) {
            g_pSwapChain->Release();
            g_pSwapChain = nullptr;
        }
        if (g_pd3dDeviceContext) {
            g_pd3dDeviceContext->Release();
            g_pd3dDeviceContext = nullptr;
        }
        if (g_pd3dDevice) {
            g_pd3dDevice->Release();
            g_pd3dDevice = nullptr;
        }
    }

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
            g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

namespace GUI
{
    static std::mutex g_stateMutex;
    static bool g_shouldExit = false;

    void GetOptimalWindowSize(int& outWidth, int& outHeight)
    {
        outWidth = 1280;
        outHeight = 800;
    }

    void RequestExit()
    {
        g_shouldExit = true;
        if (g_hWnd)
            ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
    }

    bool Initialize()
    {
        g_wc = { sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"NostalgiaWndClass", nullptr };
        if (!::RegisterClassExW(&g_wc))
            return false;
        g_windowClassRegistered = true;

        int w = 1280, h = 800;
        GetOptimalWindowSize(w, h);
        RECT rc = { 0, 0, w, h };
        ::AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

        g_hWnd = ::CreateWindowExW(0, g_wc.lpszClassName, L"Nostalgia Dumper", WS_OVERLAPPEDWINDOW, 100, 100,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, g_wc.hInstance, nullptr);
        if (!g_hWnd) {
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
            g_windowClassRegistered = false;
            return false;
        }

        if (!CreateDeviceD3D(g_hWnd)) {
            CleanupDeviceD3D();
            ::DestroyWindow(g_hWnd);
            g_hWnd = nullptr;
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
            g_windowClassRegistered = false;
            return false;
        }

        ::ShowWindow(g_hWnd, SW_SHOWDEFAULT);
        ::UpdateWindow(g_hWnd);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
        return true;
    }

    void Shutdown()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        if (g_hWnd) {
            ::DestroyWindow(g_hWnd);
            g_hWnd = nullptr;
        }
        if (g_windowClassRegistered) {
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
            g_windowClassRegistered = false;
        }
    }

    void ShowError(AppState& appState, const std::string& title, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        appState.errorTitle = title;
        appState.errorMessage = message;
        appState.showErrorDialog = true;
    }

    void RefreshProcessList(AppState& appState)
    {
        std::vector<ProcessInfo> procs;
        Memory::EnumerateProcesses("", procs);
        std::lock_guard<std::mutex> lock(g_stateMutex);
        appState.availableProcesses = std::move(procs);
    }

    void RequestScan(AppState& appState)
    {
        if (appState.isScanning)
            return;

        if (appState.scanThread) {
            if (appState.scanThread->joinable())
                appState.scanThread->join();
            delete appState.scanThread;
            appState.scanThread = nullptr;
        }

        AppState* st = &appState;
        const u32 pid = appState.selectedProcessId;
        const int selEngine = appState.selectedEngine;
        const bool deep = appState.deepUeScan;

        appState.isScanning = true;
        appState.scanComplete = false;
        appState.scanProgress = 0.0f;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            appState.foundOffsets = DiscoveredOffsets();
            appState.engineSymbols.clear();
            appState.scanLog.clear();
        }

        appState.scanThread = new std::thread([st, pid, selEngine, deep]() {
            DiscoveredOffsets offsets;
            std::map<std::string, u64> symbols;
            std::string log;
            const auto target = static_cast<EngineScans::Target>(selEngine);
            const Result r = EngineScans::Run(target, pid, offsets, symbols, log, deep);

            const bool ok = r.IsSuccess();
            std::string err = r.message;
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                st->foundOffsets = std::move(offsets);
                st->engineSymbols = std::move(symbols);
                st->scanLog = std::move(log);
                st->isScanning = false;
                st->scanProgress = 1.0f;
                st->scanComplete = ok;
                st->completedScanEngine = selEngine;
                st->showScanCompletePopup = ok;
            }
            if (!ok)
                ShowError(*st, "Scan failed", err.empty() ? "Unknown error" : err);
        });
    }

    void RequestHeaderGeneration(AppState& appState)
    {
        const std::string path(appState.outputPathBuffer);
        if (path.empty()) {
            ShowError(appState, "Invalid path", "Set an output file path (e.g. C:\\\\temp\\\\offsets.h).");
            return;
        }

        const std::string gameName = appState.selectedProcessName.empty() ? "Game" : appState.selectedProcessName;
        static const char* kEngineTags[] = { "unreal", "unity_il2cpp", "unity_mono", "source1", "source2", "godot",
            "cryengine", "universal", "gamemaker", "unigine" };
        constexpr int kEngineTagCount = static_cast<int>(sizeof(kEngineTags) / sizeof(kEngineTags[0]));
        const char* engineTag = (appState.completedScanEngine >= 0 && appState.completedScanEngine < kEngineTagCount)
            ? kEngineTags[appState.completedScanEngine]
            : "unknown";

        std::string content;
        std::string json;
        if (appState.completedScanEngine == 0)
        {
            const std::map<std::string, u64>* extras = appState.engineSymbols.empty() ? nullptr : &appState.engineSymbols;
            content = Dumper::GenerateHeaderContent(appState.foundOffsets, gameName, extras);
            json = Dumper::GenerateFullDumpJson(appState.foundOffsets, gameName, extras);
        }
        else
        {
            content = Dumper::GenerateSymbolPackHeader(appState.engineSymbols, gameName, engineTag);
            json = Dumper::GenerateSymbolPackJson(appState.engineSymbols, engineTag, gameName);
        }

        std::ofstream f(path, std::ios::binary);
        if (!f) {
            ShowError(appState, "Write failed", "Could not write to:\n" + path);
            return;
        }
        f << content;

        namespace fs = std::filesystem;
        const fs::path outp(path);
        const fs::path jsonPath = outp.has_parent_path() ? (outp.parent_path() / (outp.stem().string() + "_dump.json"))
                                                         : fs::path(outp.stem().string() + "_dump.json");
        std::ofstream jf(jsonPath.string(), std::ios::binary);
        if (jf)
            jf << json;

        appState.generatedHeaderContent = content;
        appState.headerGenerated = true;
        appState.showScanCompletePopup = true;
        if (!jf)
            ShowError(appState, "Partial export", "Header written, but JSON failed:\n" + jsonPath.string());
    }

    void SetDarkTheme()
    {
        auto& style = ImGui::GetStyle();
        auto& colors = style.Colors;

        style.WindowRounding = 10.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(12, 12);
        style.WindowPadding = ImVec2(15, 15);

        ImVec4 bg = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
        ImVec4 accent = ImVec4(0.12f, 0.45f, 1.00f, 1.00f);
        ImVec4 accentHover = ImVec4(0.18f, 0.52f, 1.00f, 1.00f);
        ImVec4 text = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
        ImVec4 textDim = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);

        colors[ImGuiCol_Text]                   = text;
        colors[ImGuiCol_TextDisabled]           = textDim;
        colors[ImGuiCol_WindowBg]               = bg;
        colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_Border]                 = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_TitleBg]                = bg;
        colors[ImGuiCol_TitleBgActive]          = bg;
        colors[ImGuiCol_CheckMark]              = accent;
        colors[ImGuiCol_SliderGrab]             = accent;
        colors[ImGuiCol_SliderGrabActive]       = accentHover;
        colors[ImGuiCol_Button]                 = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = accent;
        colors[ImGuiCol_ButtonActive]           = accentHover;
        colors[ImGuiCol_Header]                 = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = accent;
        colors[ImGuiCol_HeaderActive]           = accentHover;
        colors[ImGuiCol_Separator]              = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TabHovered]             = accent;
        colors[ImGuiCol_TabActive]              = accent;
    }

    bool RenderFrame(AppState& appState)
    {
        MSG msg = {};
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                return false;
        }
        if (g_shouldExit || appState.shouldClose)
            return false;

        if (g_resizeWidth != 0 && g_resizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeWidth = g_resizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!appState.changelogShown) {
            appState.showChangelog = true;
            appState.changelogShown = true;
        }

        // Apply global opacity to the window background
        ImGui::SetNextWindowBgAlpha(appState.uiOpacity);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        
        ImGui::Begin("NostalgiaMain", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Custom Topbar ---
        const float topbarHeight = 35.0f;
        ImGui::BeginChild("Topbar", ImVec2(0, topbarHeight), false, ImGuiWindowFlags_NoScrollbar);
        
        // Draggable area logic
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            appState.isDragging = true;
            appState.dragOffset = ImGui::GetMousePos();
        }
        if (appState.isDragging && !ImGui::IsMouseDown(0)) appState.isDragging = false;
        
        ImGui::SetCursorPos(ImVec2(10, 8));
        ImGui::TextColored(ImVec4(0.12f, 0.45f, 1.00f, 1.00f), "NOSTALGIA DUMPER");
        ImGui::SameLine();
        ImGui::TextDisabled("| v%s", NOSTALGIA_VERSION_STR);

        ImGui::EndChild();
        ImGui::Separator();

        // --- Sidebar ---
        const float sidebarWidth = 320.0f;
        ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::Spacing();
        ImGui::TextDisabled("ENGINE CONFIGURATION");
        ImGui::BeginChild("EngineCard", ImVec2(0, 100), true);
        const char* engines[] = { "Unreal Engine", "Unity (IL2CPP)", "Unity (Mono)", "Source", "Source 2", "Godot Engine", "CryEngine", "Universal", "GameMaker Studio", "UNIGINE" };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##Engine", engines[appState.selectedEngine]))
        {
            for (int i = 0; i < 10; i++)
            {
                if (ImGui::Selectable(engines[i], appState.selectedEngine == i))
                    appState.selectedEngine = i;
            }
            ImGui::EndCombo();
        }
        if (appState.selectedEngine == 0)
            ImGui::Checkbox("Deep Scan Mode", &appState.deepUeScan);
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextDisabled("TARGET PROCESS");
        ImGui::BeginChild("ProcessCard", ImVec2(0, 320), true);
        if (ImGui::Button("Refresh List", ImVec2(-1, 0)))
            RefreshProcessList(appState);
        
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Filter", "Search processes...", appState.processSearchBuffer, sizeof(appState.processSearchBuffer));

        ImGui::BeginChild("ProcessListInner", ImVec2(0, 220), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& proc : appState.availableProcesses)
        {
            if (appState.processSearchBuffer[0] != '\0') {
                std::string name = proc.processName;
                std::string filter = appState.processSearchBuffer;
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (name.find(filter) == std::string::npos) continue;
            }
            bool isSelected = (appState.selectedProcessId == proc.processId);
            if (ImGui::Selectable(proc.processName.c_str(), isSelected)) {
                appState.selectedProcessId = proc.processId;
                appState.selectedProcessName = proc.processName;
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextDisabled("EXECUTION");
        ImGui::BeginChild("ActionCard", ImVec2(0, 140), true);
        ImGui::BeginDisabled(appState.selectedProcessId == 0 || appState.isScanning);
        if (ImGui::Button(appState.isScanning ? "SCANNING..." : "START DISCOVERY", ImVec2(-1, 45)))
            RequestScan(appState);
        ImGui::EndDisabled();

        if (appState.isScanning) {
            ImGui::Spacing();
            ImGui::ProgressBar(appState.scanProgress, ImVec2(-1, 15));
        }
        ImGui::EndChild();

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 80);
        if (ImGui::Button("QUIT", ImVec2(-1, 30))) appState.shouldClose = true;

        ImGui::EndChild();

        ImGui::SameLine();

        // --- Main Content ---
        ImGui::BeginChild("MainContent", ImVec2(0, 0), true);
        
        if (ImGui::BeginTabBar("NostalgiaTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("DASHBOARD"))
            {
                ImGui::Spacing();
                ImGui::TextDisabled("SESSION OVERVIEW");
                ImGui::Separator();
                
                ImGui::Columns(2, "StatusColumns", false);
                ImGui::BeginChild("StatusCard1", ImVec2(0, 80), true);
                ImGui::TextDisabled("ACTIVE PROCESS");
                ImGui::Text(appState.selectedProcessId != 0 ? appState.selectedProcessName.c_str() : "None Selected");
                ImGui::EndChild();
                
                ImGui::NextColumn();
                
                ImGui::BeginChild("StatusCard2", ImVec2(0, 80), true);
                ImGui::TextDisabled("ENGINE TARGET");
                ImGui::Text(engines[appState.selectedEngine]);
                ImGui::EndChild();
                ImGui::Columns(1);

                ImGui::Spacing();
                ImGui::TextDisabled("RECENT DUMPS");
                ImGui::Separator();
                ImGui::BeginChild("RecentsCard", ImVec2(0, 180), true);
                if (appState.recentDumps.empty()) {
                    ImGui::VerticalSpacing(60);
                    TextCentered("No recent dumps found.");
                } else {
                    if (ImGui::BeginTable("RecentsTable", 4, ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Game");
                        ImGui::TableSetupColumn("Engine");
                        ImGui::TableSetupColumn("Date");
                        ImGui::TableSetupColumn("Found");
                        ImGui::TableHeadersRow();
                        for (const auto& r : appState.recentDumps) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r.gameName.c_str());
                            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.engine.c_str());
                            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.date.c_str());
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", r.foundCount);
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::TextDisabled("EXPORT CONFIGURATION");
                ImGui::Separator();
                ImGui::BeginChild("ExportCard", ImVec2(0, 120), true);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##Path", "C:\\\\out\\\\offsets.h (writes offsets + stem_dump.json)", appState.outputPathBuffer, sizeof(appState.outputPathBuffer));
                ImGui::Spacing();
                ImGui::BeginDisabled(!appState.scanComplete);
                if (ImGui::Button("GENERATE OFFSET HEADER + JSON DUMP", ImVec2(-1, 35)))
                    RequestHeaderGeneration(appState);
                ImGui::EndDisabled();
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("DISCOVERED DATA"))
            {
                if (appState.scanComplete) {
                    ImGui::InputTextWithHint("##GlobalFilter", "Filter results...", appState.globalFilter, sizeof(appState.globalFilter));
                    if (ImGui::BeginTable("DataTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
                        ImGui::TableSetupColumn("Identifier");
                        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 132.0f);
                        ImGui::TableSetupColumn("Signature / note", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                        ImGui::TableHeadersRow();

                        auto filterMatch = [&](const char* name, const std::string& sigLine) {
                            if (appState.globalFilter[0] == '\0')
                                return true;
                            std::string n = name;
                            std::string f = appState.globalFilter;
                            std::string s = sigLine;
                            std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                            std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                            return n.find(f) != std::string::npos || s.find(f) != std::string::npos;
                        };

                        auto addOffsetRow = [&](const char* name, const OffsetInfo& inf) {
                            const std::string sigDisp = inf.signature.empty() ? "-" : inf.signature;
                            if (!filterMatch(name, sigDisp))
                                return;
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
                            ImGui::TableSetColumnIndex(1);
                            if (inf.address) ImGui::Text("0x%llX", inf.address); else ImGui::TextDisabled("Not Found");
                            ImGui::TableSetColumnIndex(2);
                            ImGui::PushTextWrapPos(0.0f);
                            ImGui::TextUnformatted(sigDisp.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::TableSetColumnIndex(3);
                            if (inf.address && ImGui::SmallButton((std::string("Copy##") + name).c_str())) {
                                char hex[40]; sprintf(hex, "0x%llX", inf.address);
                                ImGui::SetClipboardText(hex);
                            }
                        };

                        auto addSymbolRow = [&](const char* name, u64 addr) {
                            if (!filterMatch(name, "export"))
                                return;
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
                            ImGui::TableSetColumnIndex(1); ImGui::Text("0x%llX", addr);
                            ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("export / harvest");
                            ImGui::TableSetColumnIndex(3);
                            if (ImGui::SmallButton((std::string("Copy##s") + name).c_str())) {
                                char hex[40]; sprintf(hex, "0x%llX", addr);
                                ImGui::SetClipboardText(hex);
                            }
                        };

                        if (appState.completedScanEngine == 0)
                        {
                            addOffsetRow("GObjects", appState.foundOffsets.GObjects);
                            addOffsetRow("GNames", appState.foundOffsets.GNames);
                            addOffsetRow("GWorld", appState.foundOffsets.GWorld);
                            addOffsetRow("GEngine", appState.foundOffsets.GEngine);
                        }
                        for (const auto& kv : appState.foundOffsets.customOffsets)
                            addOffsetRow(kv.first.c_str(), kv.second);
                        for (const auto& [name, addr] : appState.engineSymbols)
                            addSymbolRow(name.c_str(), addr);
                        ImGui::EndTable();
                    }
                } else {
                    ImGui::VerticalSpacing(100);
                    TextCentered("No data discovered yet.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("FORENSICS"))
            {
                if (ImGui::BeginTabBar("ForensicsTabs")) {
                    if (ImGui::BeginTabItem("Pattern Finder")) {
                        ImGui::InputTextWithHint("##Pattern", "48 8B 05 ? ? ? ?", appState.customPattern, sizeof(appState.customPattern));
                        ImGui::InputTextWithHint("##Module", "Engine.dll", appState.customModule, sizeof(appState.customModule));
                        ImGui::InputInt("Offset", &appState.customOffset);
                        if (ImGui::Button("SCAN PATTERN", ImVec2(-1, 35))) {
                            HANDLE h = Memory::OpenProcessHandle(appState.selectedProcessId);
                            if (h) {
                                u64 res = Patterns::ScanModule(h, appState.customModule, appState.customPattern, appState.customOffset);
                                if (res) appState.customScanResult = "Found: 0x" + std::to_string(res);
                                else appState.customScanResult = "Not Found";
                                Memory::CloseProcessHandle(h);
                            }
                        }
                        ImGui::TextUnformatted(appState.customScanResult.c_str());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Module Map")) {
                        std::vector<ModuleInfo> mods;
                        HANDLE h = Memory::OpenProcessHandle(appState.selectedProcessId);
                        if (h) {
                            Memory::EnumerateModules(h, mods);
                            if (ImGui::BeginTable("ModuleTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                                ImGui::TableSetupColumn("Name");
                                ImGui::TableSetupColumn("Base");
                                ImGui::TableSetupColumn("Size");
                                ImGui::TableHeadersRow();
                                for (const auto& m : mods) {
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(m.name.c_str());
                                    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%llX", m.base);
                                    ImGui::TableSetColumnIndex(2); ImGui::Text("0x%X", m.imageSize);
                                }
                                ImGui::EndTable();
                            }
                            Memory::CloseProcessHandle(h);
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("SETTINGS"))
            {
                ImGui::Spacing();
                ImGui::TextDisabled("APPLICATION SETTINGS");
                ImGui::Separator();
                
                ImGui::SliderFloat("UI Opacity", &appState.uiOpacity, 0.1f, 1.0f);
                ImGui::Checkbox("Auto-Export on Finish", &appState.autoExport);
                ImGui::Checkbox("Multi-Threaded Scanning", &appState.multiThreadedScan);
                ImGui::Checkbox("Heuristic Fallbacks", &appState.heuristicFallback);
                if (ImGui::Button("Show Changelog", ImVec2(-1, 30))) appState.showChangelog = true;
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        // --- Popups ---
        if (appState.showChangelog) {
            ImGui::OpenPopup("Changelog");
            appState.showChangelog = false;
        }
        if (ImGui::BeginPopupModal("Changelog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            char pcName[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD size = sizeof(pcName);
            if (!GetComputerNameA(pcName, &size)) strcpy(pcName, "User");

            ImGui::Text("Welcome %s to Nostalgia Dumper!", pcName);
            ImGui::Text("Please send a review to me, or star the GitHub Repository!");
            ImGui::Separator();
            ImGui::Text("Nostalgia Dumper v1.9.0 Update");
            ImGui::Separator();
            ImGui::Text("- Optimized all engine dumpers for peak performance");
            ImGui::Text("- Fixed Unreal Engine offset discovery (GObjects, GWorld, etc.)");
            ImGui::Text("- Improved multi-module scanning for all engines");
            ImGui::Text("- Added GameMaker Studio and UNIGINE support");
            ImGui::Text("- Added Personalized Welcome Message");
            ImGui::Spacing();
            if (ImGui::Button("CLOSE", ImVec2(-1, 30))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (appState.showErrorDialog) {
            ImGui::OpenPopup("ErrorModal");
            appState.showErrorDialog = false;
        }
        if (ImGui::BeginPopupModal("ErrorModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(appState.errorTitle.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", appState.errorMessage.c_str());
            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (appState.showScanCompletePopup) {
            ImGui::OpenPopup("DoneModal");
            appState.showScanCompletePopup = false;
        }
        if (ImGui::BeginPopupModal("DoneModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Finished successfully.");
            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::Render();
        const float clear_color[4] = { 0.06f, 0.06f, 0.08f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
        return true;
    }

    void TextCentered(const char* text) {
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::TextUnformatted(text);
    }
}

namespace ImGui {
    void VerticalSpacing(float size) { ImGui::Dummy(ImVec2(0.0f, size)); }
}
