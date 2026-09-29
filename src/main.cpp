// main.cpp
// Entry point
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_freetype.h>
#include <d3d11.h>
#include <tchar.h>
#include <implot.h>
#include <tracy/Tracy.hpp>

#include "config/FedReleaseCatalog.hpp"

// REMOVE : way to check version
// Hover over "_MSVC_LANG" to check language server version
auto current_standard = _MSVC_LANG;
#include "custom/keynav.h"

#include "core/AlertBus.hpp"


#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    char buf[256];
    snprintf(buf, sizeof(buf), 
        "CRASH: Exception 0x%08X at address 0x%p\n\nPress OK to exit.",
        code, addr);
    MessageBoxA(nullptr, buf, "fed_terminal crashed", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

#include <UIStyle.hpp>

#include "panels/RatePanel.hpp"
#include "panels/SecfilingPanel.hpp"
#include "panels/FedPanel.hpp"

#include "config/AppConfig.hpp"
#include "config/RateConfigs.hpp"
#include "config/FedConfig.hpp"

#include "core/RegisterPanels.hpp"
#include "core/PanelRegistry.hpp"

#include "ui/Commandbar.hpp"
#include "core/CommandRegistry.hpp"
#include "core/WatchlistManager.hpp"
#include "rss/RSSPoller.hpp"

#include "networking/NetworkClient.hpp"

#include "rendering/tile_renderer.hpp"
#include <Ultralight/Ultralight.h>
#include <AppCore/Platform.h>

ImFont* g_TerminalFont = nullptr;

// Forward declarations
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DirectX globals
ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;


// Forward declarations
void CreateRenderTarget();
void CleanupRenderTarget();
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();

// Implementations
void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };
    if (D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
            featureLevelArray, 1, D3D11_SDK_VERSION,
            &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel,
            &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0; // Disable ALT menu
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // debug console, remove for release
    AllocConsole();
    freopen("CONOUT$", "w", stdout);

    SetUnhandledExceptionFilter(CrashHandler);

    if (!AppConfig::Instance().Load("config.local")) {
        MessageBoxA(nullptr, "config.local not found.\nCopy config.example and fill in your variables.", "Config Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Setup window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("FedTerminal"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Fed Terminal"),
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800,
        NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

    // Large atlas to avoid overflow
    io.Fonts->TexMinWidth = 4096;
    io.Fonts->TexMaxWidth = 4096;

    // TODO replace this with user controlled font selection
    g_TerminalFont = io.Fonts->AddFontFromFileTTF("fonts/SFMonoRegular.otf", 20.0f);

    if (g_TerminalFont == nullptr) {
        printf("Warning: Could not find SF Mono.\n");
    }

	ImPlot::CreateContext();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    Rendering::TileRasterizer::SetDevice(g_pd3dDevice, g_pd3dDeviceContext);

    ultralight::Platform::instance().set_config({});
    ultralight::Platform::instance().set_font_loader(
        ultralight::GetPlatformFontLoader());
    ultralight::Platform::instance().set_file_system(
        ultralight::GetPlatformFileSystem("."));
    ultralight::Platform::instance().set_logger(
        ultralight::GetDefaultLogger("ultralight.log"));

    ultralight::RefPtr<ultralight::Renderer> ul_renderer =
        ultralight::Renderer::Create();

    UI::ApplyTheme();

    // TODO: This needs proper user selectable logic instead of hardcoding
    std::vector<RateClient> clients = {
        makeEFFR("2025-10-10", "2025-10-17"),
        makeSOFR("2025-10-10", "2025-10-17"),
        makeRP(20),
        makeRRP(20),
    };
    for (auto& c : clients)
        c.fetch();

    // TODO: Fed data related stuff is being reworked, all this will be gonne
    const std::string& fedkey = AppConfig::Instance().Get("FED_API_KEY");
    static FedClient g_fed = makeFedH41(fedkey);
    std::thread([] { g_fed.fetchAll();}).detach();

    RegisterAllPanels(ul_renderer.get());
    PanelRegistry::Get().LoadUserData("userdata.json");
    PanelRegistry::Get().LoadLayout("layout.txt");
    WatchlistManager::Get().Load("watchlists.json");
    FedReleaseCatalog::Get().LoadFromFile("assets/fed_mappings.json");
    RSSPoller::Get().Load("rss_users.json");
    RSSPoller::Get().Start(60);
    
    // [TEMP] I generally dont like this
    NetworkClient::Get().Connect("127.0.0.1", 12345);

    bool done = false;
    while (!done)
    {
        ZoneScopedN("Main frame")
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static bool s_showMetrics = false;
        static bool s_showStyle   = false;

        ImGui::Begin("Debugger");
        if (ImGui::Button("Metrics"))   s_showMetrics = !s_showMetrics;
        ImGui::SameLine();
        if (ImGui::Button("Style"))     s_showStyle   = !s_showStyle;
        ImGui::End();

        if (s_showMetrics) ImGui::ShowMetricsWindow(&s_showMetrics);
        if (s_showStyle)   ImGui::ShowStyleEditor();

        RenderCommandBar();

        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_NoWindowMenuButton;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
		float barH = ImMax(io.DisplaySize.y * 0.035f, 36.0f);
        ImGui::SetNextWindowPos(ImVec2(0.0f, barH));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - barH));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        ImGui::End();

        // TODO: This is just for testing, will be removed
        ImGui::Begin("Feed Tester");

        if (ImGui::Button("Simulate SEC Filing")) {
            AlertBus::Get().Post(AlertType::SEC, "SEC", "New Form 4: Elon Musk (TSLA) - Sold 1,000,000 shares");
        }

        if (ImGui::Button("Simulate FED News")) {
            AlertBus::Get().Post(AlertType::FED, "FED", "Jerome Powell: 'Inflation remains elevated, rates steady.'");
        }

        if (ImGui::Button("Simulate Error")) {
            AlertBus::Get().Post(AlertType::Error, "SYSTEM", "Connection to EDGAR lost. Retrying in 5s...");
        }

        if (ImGui::Button("Spam 10 Alerts")) {
            for (int i = 0; i < 10; i++) {
                AlertBus::Get().Post(AlertType::System, "SYSTEM", "Spam alert sequence " + std::to_string(i));
            }
        }

        ImGui::End(); // REMOVE UNTILL HERE, delete AlertBus.hpp include

        // WIP: EnTT implementation, PanelRegistry will replace CommandRegistry once all panels are reworked with EnTT
        PanelRegistry::Get().DrawAll();
        //CommandRegistry::Get().RenderAll();

        HandleDockNavKeys();
        DrawKeyNavDebug();

        ImGui::Render();
        const float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        FrameMark;
    }
    PanelRegistry::Get().SaveLayout("layout.txt");
    PanelRegistry::Get().SaveUserData("userdata.json");
    RSSPoller::Get().Stop();
    NetworkClient::Get().Disconnect();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	ImPlot::DestroyContext();
	CleanupDeviceD3D();
	    DestroyWindow(hwnd);
	    UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 0;
    }
