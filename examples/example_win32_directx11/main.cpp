#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include "fonts.h"
#include <d3d11.h>
#include <tchar.h>
#include <dwmapi.h>

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

//Helpers
ImDrawList* drawlist;
ImVec2 pos;
int tabs = 0;
int select_defaults = 0;
ImFont* mainfont;
ImFont* titlefont;
ImFont* icons;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    WNDCLASSEXW wc;
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = nullptr;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = L"Loader";
    wc.lpszClassName = L"Loader";
    wc.hIconSm = LoadIcon(0, IDI_APPLICATION);

    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(NULL, wc.lpszClassName, L"Loader", WS_POPUP, (GetSystemMetrics(SM_CXSCREEN) / 2) - (650 / 2), (GetSystemMetrics(SM_CYSCREEN) / 2) - (800 / 2), 650, 800, 0, 0, 0, 0);
    SetWindowLongA(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    POINT mouse;
    RECT rc = { 0 };
    GetWindowRect(hwnd, &rc);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    mainfont = io.Fonts->AddFontFromMemoryTTF(&mainfonthxd, sizeof mainfonthxd, 15, NULL, io.Fonts->GetGlyphRangesCyrillic());
    titlefont = io.Fonts->AddFontFromMemoryTTF(&mainfonthxd, sizeof mainfonthxd, 24, NULL, io.Fonts->GetGlyphRangesCyrillic());
    icons = io.Fonts->AddFontFromMemoryTTF(&iconshxd, sizeof iconshxd, 20, NULL, io.Fonts->GetGlyphRangesCyrillic());

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        static bool hide = true;
        static int opticaly = 255;
        if (GetAsyncKeyState(VK_F12) & 0x1) hide = !hide;
        opticaly = ImLerp(opticaly, opticaly <= 255 && hide ? 300 : 0, ImGui::GetIO().DeltaTime * 8.f);
        if (opticaly > 255) opticaly = 255;
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), opticaly, LWA_ALPHA);
        ShowWindow(hwnd, opticaly > 0 ? SW_SHOW : SW_HIDE);
        ImGui::NewFrame();
        {
            ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
            {
                //sizing
                ImGui::SetWindowPos(ImVec2(0, 0));
                ImGui::SetWindowSize(ImVec2(650, 800));

                //helpers def
                drawlist = ImGui::GetWindowDrawList();
                pos = ImGui::GetWindowPos();

                //title
                ImGui::PushFont(titlefont);
                ImGui::SetCursorPos(ImVec2(15, 15));
                ImGui::TextColored(ImColor(127, 125, 126), "Menu");
                ImGui::PopFont();

                //tabs
                ImGui::SetCursorPos(ImVec2(20, 70));
                ImGui::BeginGroup();
                {
                    if (ImGui::Tabs("Aimbot", ImVec2(95, 25), tabs == 0)) tabs = 0;
                    if (ImGui::Tabs("Player ESP", ImVec2(125, 25), tabs == 1)) tabs = 1;
                    if (ImGui::Tabs("Item ESP", ImVec2(110, 25), tabs == 2)) tabs = 2;
                    if (ImGui::Tabs("Radar", ImVec2(80, 25), tabs == 3)) tabs = 3;
                    if (ImGui::Tabs("VIP", ImVec2(60, 25), tabs == 4)) tabs = 4;
                    if (ImGui::Tabs("Settings", ImVec2(105, 25), tabs == 5)) tabs = 5;
                }
                ImGui::EndGroup();
                drawlist->AddLine(ImVec2(pos.x, pos.y + 95), ImVec2(pos.x + 650, pos.y + 95), ImColor(107, 105, 106, 255));

                ImGui::SetCursorPos(ImVec2(15, 120));
                ImGui::Text("Defaults: ");
                ImGui::SetCursorPos(ImVec2(80, 120));
                if (ImGui::Defaults("Legit", ImVec2(65, 25), select_defaults == 0))
                    select_defaults = 0;
                ImGui::SetCursorPos(ImVec2(155, 120));
                if (ImGui::Defaults("Semi Legit", ImVec2(100, 25), select_defaults == 1))
                    select_defaults = 1;
                ImGui::SetCursorPos(ImVec2(265, 120));
                if (ImGui::Defaults("Rage", ImVec2(65, 25), select_defaults == 2))
                    select_defaults = 2;

                ImGui::SetCursorPosY(155);
                ImGui::Separator();

                //functions
                ImGui::PushItemWidth(210);
                if (tabs == 0)
                {
                    ImGui::SetCursorPos(ImVec2(15, 170));
                    ImGui::BeginGroup();
                    {
                        //functions for the demo
                        static bool checkbox[20];
                        static int sliderint0 = 0;
                        static float sliderfloat = 0.f;
                        static int combo = 0;
                        static char input0[64] = "";
                        static char text[1024 * 8] =
                            "/*\n"
                            " The Pentium F00F bug, shorthand for F0 0F C7 C8,\n"
                            " the hexadecimal encoding of one offending instruction,\n"
                            " more formally, the invalid operand with locked CMPXCHG8B\n"
                            " instruction bug, is a design flaw in the majority of\n";

                        ImGui::Checkbox("Enable Aimbot", &checkbox[0]);
                        ImGui::Checkbox("No Recoil", &checkbox[1]);
                        ImGui::Checkbox("No Spread", &checkbox[2]);
                        ImGui::Checkbox("Headshot Only", &checkbox[3]);

                        ImGui::SliderInt("Slider Int", &sliderint0, 0, 100);
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(320, -37)));
                        if (ImGui::Switcher("A##sliderint", ImVec2(29, 21)))
                            sliderint0--;
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(590, -37)));
                        if (ImGui::Switcher("B##sliderint", ImVec2(29, 21)))
                            sliderint0++;

                        ImGui::SliderFloat("Slider Float", &sliderfloat, 0, 100);
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(320, -37)));
                        if (ImGui::Switcher("A##sliderfloat", ImVec2(29, 21)))
                            sliderfloat--;
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(590, -37)));
                        if (ImGui::Switcher("B##sliderfloat", ImVec2(29, 21)))
                            sliderfloat++;

                        ImGui::Combo("Combo", &combo, "Select 1\0\Select 2\0\Select 3", 3);
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(320, -37)));
                        if (ImGui::Switcher("A##combo", ImVec2(29, 21)))
                            combo--;
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos() + ImVec2(590, -37)));
                        if (ImGui::Switcher("B##comboo", ImVec2(29, 21)))
                            combo++;

                        ImGui::Checkbox("Checkbox", &checkbox[4]);
                        ImGui::Checkbox("Checkbox", &checkbox[5]);
                        ImGui::Checkbox("Checkbox", &checkbox[6]);
                        ImGui::Checkbox("Checkbox", &checkbox[7]);
                        ImGui::Checkbox("Checkbox", &checkbox[8]);
                        ImGui::Checkbox("Checkbox", &checkbox[9]);
                        ImGui::Text("Hotkey example");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(335);
                        ImGui::Button("Button Example", ImVec2(300, 25));
                        ImGui::InputTextWithHint("Input Text", "Input text hint", input0, 64);
                        ImGui::InputTextMultiline("Source", text, IM_ARRAYSIZE(text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8), ImGuiInputTextFlags_None);
                    }
                    ImGui::EndGroup();
                }
                ImGui::PopItemWidth();

                ImGui::SetCursorPos(ImVec2(0, 0));
                if (ImGui::InvisibleButton("Fill", ImVec2(650, 800)));
                if (ImGui::IsItemActive()) {

                    GetWindowRect(hwnd, &rc);
                    MoveWindow(hwnd, rc.left + ImGui::GetMouseDragDelta().x, rc.top + ImGui::GetMouseDragDelta().y, 650, 800, TRUE);
                }
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
