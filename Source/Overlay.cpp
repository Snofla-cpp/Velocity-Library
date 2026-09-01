#include "Overlay.h"
#include "Config.h"
#include "Menu.h"
#include "Memory.h"
#include "Glow.h"
#include "Offsets.h"
#include "xorstr.hpp"
#include "Weapon.h"              // <-- för GetWeaponNameFromPawn

#include "Bunnyhop.h"
#include "NoFlash.h"
#include "FOV.h"
#include "Aimbot.h"
#include "BombTimer.h"
#include "Radar.h"
#include "Render.h"
#include "Triggerbot.h"
#include "HeadShotLine.h"

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ─── Frame pacing ─────────────────────────────────────────────────────────
static constexpr int TARGET_FPS = 144;
static constexpr int MENU_FPS = 240;

// ─── Globala ──────────────────────────────────────────────────────────────
ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_pRTV = nullptr;

static HWND     g_overlay = nullptr;
static HWND     g_gameWnd = nullptr;
static bool     g_running = true;
static int      g_width = 1920;
static int      g_height = 1080;

bool g_unload_requested = false;

// ─── EntityCache ──────────────────────────────────────────────────────────
EntityCache g_entityCache;

// ─── Bakgrundstråd för cache ──────────────────────────────────────────────
static std::thread g_cacheThread;
static std::atomic<bool> g_cacheRunning{ false };

// ─── Implementering av EntityCache ──────────────────────────────────────
void EntityCache::Update() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Nollställ alla poster
    for (auto& e : m_entities) e.valid = false;

    uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
    uintptr_t localController = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerController);
    if (!localPawn || !localController) return;

    uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);
    uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
    if (!entityList) return;

    Vector3 localOrigin{};
    uintptr_t localSceneNode = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
    if (localSceneNode)
        localOrigin = mem.Read<Vector3>(localSceneNode + offsets::sceneNode::m_vecAbsOrigin);

    bool needBones = config.bEspSkeleton || config.bEspHeadCircle || config.bEspViewDirection;

    int idx = 0;
    for (int i = 1; i <= 64 && idx < MAX_PLAYERS; ++i) {
        uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9) + 16));
        if (!listEntry) continue;

        uintptr_t controller = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
        if (!controller || controller < 0x10000) continue;
        if (controller == localController) continue;

        uint32_t pawnHandle = mem.Read<uint32_t>(controller + offsets::controller::m_hPlayerPawn);
        if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;

        uintptr_t pawnEntry = mem.Read<uintptr_t>(entityList + (8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
        if (!pawnEntry) continue;

        uintptr_t pawn = mem.Read<uintptr_t>(pawnEntry + 112 * ((pawnHandle & 0x7FFF) & 0x1FF));
        if (!pawn || pawn == localPawn) continue;

        int health = mem.Read<int>(pawn + offsets::entity::m_iHealth);
        uint8_t team = mem.Read<uint8_t>(pawn + offsets::entity::m_iTeamNum);
        if (health <= 0 || health > 100) continue;
        if (config.bEspTeamCheck && team == localTeam) continue;

        uintptr_t sceneNode = mem.Read<uintptr_t>(pawn + offsets::entity::m_pGameSceneNode);
        if (!sceneNode) continue;
        if (mem.Read<bool>(sceneNode + offsets::sceneNode::m_bDormant)) continue;

        Vector3 origin = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
        float distance = localOrigin.Distance(origin);

        int armor = mem.Read<int>(pawn + offsets::csPawn::m_ArmorValue);
        if (armor == 0) armor = mem.Read<int>(controller + offsets::controller::m_iPawnArmor);

        // Fyll i cache-posten
        CachedEntity& ent = m_entities[idx];
        ent.valid = true;
        ent.controller = controller;
        ent.pawn = pawn;
        ent.health = health;
        ent.armor = armor;
        ent.team = team;
        ent.origin = origin;
        ent.distance = distance;

        // Namn
        char nameBuffer[128] = {};
        mem.ReadRaw(controller + offsets::controller::m_iszPlayerName, nameBuffer, sizeof(nameBuffer) - 1);
        strncpy_s(ent.name, nameBuffer, sizeof(ent.name) - 1);

        // Vapen
        if (config.bEspWeapon) {
            if (!GetWeaponNameFromPawn(pawn, ent.weaponName, sizeof(ent.weaponName)))
                strcpy_s(ent.weaponName, "?");
        }
        else {
            ent.weaponName[0] = '\0';
        }

        // View angles (för ViewDirection)
        ent.viewAngles = mem.Read<Vector3>(pawn + offsets::csPawn::m_angEyeAngles);  // använd rätt offset

        // Ben (endast vid behov)
        if (needBones) {
            uintptr_t boneMatrix = mem.Read<uintptr_t>(sceneNode + offsets::skeleton::m_modelState + 0x80);
            if (boneMatrix) {
                for (int b = 0; b < BoneIndex::BONE_COUNT; ++b)
                    ent.bones[b] = mem.Read<Vector3>(boneMatrix + b * 32);
                ent.headPos = ent.bones[BoneIndex::HEAD];
                ent.bonesValid = true;
            }
            else {
                ent.headPos = origin + Vector3{ 0, 0, 72.0f };
                ent.bonesValid = false;
            }
        }
        else {
            ent.headPos = origin + Vector3{ 0, 0, 72.0f };
            ent.bonesValid = false;
        }

        ++idx;
    }
}

const std::array<CachedEntity, EntityCache::MAX_PLAYERS>& EntityCache::Get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entities;
}

// ─── Bakgrundstrådens funktion ────────────────────────────────────────────
static void CacheThreadFunc() {
    while (g_cacheRunning) {
        g_entityCache.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // 100 Hz
    }
}

// ─── Övriga hjälpfunktioner (oförändrade) ────────────────────────────────
static void WaitForNextFrame(std::chrono::steady_clock::time_point& frameStart, int targetFps) {
    using namespace std::chrono;
    auto frameDuration = microseconds(1'000'000 / targetFps);
    auto nextFrame = frameStart + frameDuration;
    std::this_thread::sleep_until(nextFrame);
    frameStart = steady_clock::now();
}

static void ForceForegroundWindow(HWND hWnd) {
    DWORD foreThreadId = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD curThreadId = GetCurrentThreadId();
    if (foreThreadId != curThreadId && foreThreadId != 0)
        AttachThreadInput(foreThreadId, curThreadId, TRUE);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    if (foreThreadId != curThreadId && foreThreadId != 0)
        AttachThreadInput(foreThreadId, curThreadId, FALSE);
}

static void FocusOverlayWindow() {
    if (!IsWindow(g_overlay)) return;
    ForceForegroundWindow(g_overlay);
}

static void UpdateMenuClipCursor(bool menuVisible) {
    if (menuVisible && IsWindow(g_overlay)) {
        RECT rect;
        GetClientRect(g_overlay, &rect);
        ClientToScreen(g_overlay, (LPPOINT)&rect);
        ClientToScreen(g_overlay, (LPPOINT)&rect + 1);
        ClipCursor(&rect);
        SetCapture(g_overlay);
    }
    else {
        ClipCursor(NULL);
        ReleaseCapture();
    }
}

static void SetStreamproof(bool enabled) {
    if (!g_overlay) return;
    SetWindowDisplayAffinity(g_overlay, enabled ? WDA_EXCLUDEFROMCAPTURE : 0);
}

// ─── DX11-hjälpare ────────────────────────────────────────────────────────
static void CreateRenderTarget() {
    ID3D11Texture2D* pBack = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    if (pBack) {
        g_pd3dDevice->CreateRenderTargetView(pBack, nullptr, &g_pRTV);
        pBack->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_pRTV) { g_pRTV->Release(); g_pRTV = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 0, 1 };
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc = { 1, 0 };
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL flArr[] = { D3D_FEATURE_LEVEL_11_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        flArr, 1, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dContext)))
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();  g_pd3dDevice = nullptr; }
}

// ─── WndProc ──────────────────────────────────────────────────────────────
static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        ClipCursor(NULL);
        ReleaseCapture();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND CreateCustomOverlay(HINSTANCE hInst, int x, int y, int w, int h) {
    const wchar_t* systemClasses[] = {
        L"tooltips_class32", L"Static", L"#32770", L"OleMainThreadWndClass"
    };
    std::wstring className = systemClasses[GetCurrentProcessId() % 4];
    className += L"." + std::to_wstring(GetTickCount64());

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className.c_str();
    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    HWND hwnd = CreateWindowExW(
        exStyle, className.c_str(), L"", WS_POPUP,
        x, y, w, h, nullptr, nullptr, hInst, nullptr);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    return hwnd;
}

// ─── ImGui-stil ────────────────────────────────────────────────────────────
static void ApplyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8, 8);
    style.WindowRounding = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildRounding = 6.0f;
    style.ChildBorderSize = 0.0f;
    style.FramePadding = ImVec2(6, 4);
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 8.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    style.PopupRounding = 4.0f;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.9686f, 0.7961f, 0.8196f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.28f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.70f, 0.70f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

// ─── Overlay::Run ──────────────────────────────────────────────────────────
void Overlay::Run() {
    MSG msg{};
    static bool lastFovState = config.bFovChanger;
    static bool lastStreamproof = config.bStreamproof;

    auto frameStart = std::chrono::steady_clock::now();

    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        if (g_unload_requested) {
            g_running = false;
            break;
        }

        // Menyhantering
        if (GetAsyncKeyState(config.menuKey) & 1) {
            config.show_menu = !config.show_menu;
            LONG_PTR exStyle = GetWindowLongPtr(g_overlay, GWL_EXSTYLE);
            if (config.show_menu) {
                exStyle &= ~WS_EX_TRANSPARENT;
                exStyle &= ~WS_EX_NOACTIVATE;
                SetWindowLongPtrW(g_overlay, GWL_EXSTYLE, exStyle);
                FocusOverlayWindow();
                UpdateMenuClipCursor(true);
                Sleep(10);
            }
            else {
                exStyle |= WS_EX_TRANSPARENT;
                exStyle |= WS_EX_NOACTIVATE;
                SetWindowLongPtrW(g_overlay, GWL_EXSTYLE, exStyle);
                UpdateMenuClipCursor(false);
                if (IsWindow(g_gameWnd)) {
                    SetForegroundWindow(g_gameWnd);
                    SetFocus(g_gameWnd);
                }
            }
        }

        if (GetAsyncKeyState(VK_END) & 1) {
            g_running = false;
            break;
        }

        if (!IsWindow(g_gameWnd)) { g_running = false; break; }
        RECT r;
        GetWindowRect(g_gameWnd, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        static RECT gr = r;
        if (w != g_width || h != g_height || r.left != gr.left || r.top != gr.top) {
            MoveWindow(g_overlay, r.left, r.top, w, h, TRUE);
            g_width = w; g_height = h;
            gr = r;
            if (config.show_menu) {
                RECT rect;
                GetClientRect(g_overlay, &rect);
                ClientToScreen(g_overlay, (LPPOINT)&rect);
                ClientToScreen(g_overlay, (LPPOINT)&rect + 1);
                ClipCursor(&rect);
            }
        }

        if (config.bStreamproof != lastStreamproof) {
            SetStreamproof(config.bStreamproof);
            lastStreamproof = config.bStreamproof;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ─── Läs lokala spelardata (direkt, få anrop) ───
        uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
        uintptr_t localController = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerController);

        // FOV
        if (localPawn) {
            FOV::WriteNow(localPawn);
        }
        if (localPawn) {
            if (config.bFovChanger != lastFovState) {
                if (!config.bFovChanger) {
                    FOV::Stop();
                    FOV::Reset(localPawn);
                }
                else {
                    FOV::Start();
                    FOV::WriteNow(localPawn);
                }
                lastFovState = config.bFovChanger;
            }
        }

        // Misc
        if (localPawn) {
            Misc::Bhop(localPawn);
            Misc::NoFlash(localPawn);
        }

        // Glow
        if (localController) {
            Glow::Run(localController);
        }

        // Aimbot
        if (localPawn && localController) {
            Aimbot::Run(localPawn, localController, g_width, g_height);
        }

        // Triggerbot
        if (localPawn) {
            uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);
            Triggerbot::Run(localPawn, localTeam);
        }

        // Radar
        if (localPawn) {
            Radar::Update();
        }

        // Bomb Timer
        BombTimer::Update();
        if (config.bBombTimer) {
            BombTimer::Draw();
        }

        // ─── Rendering (använder cachen) ────────────────────────────────
        menu::Render();
        RenderESP(g_width, g_height);

        Radar::Draw();

        // HeadShot Line (läser viewMatrix)
        if (config.bHeadShotLine && localPawn) {
            Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(mem.client + offsets::client::dwViewMatrix);
            HeadShotLine::Draw(localPawn, viewMatrix, g_width, g_height, config.headShotColor);
        }

        // FOV-ellips
        if (config.bAimbot && config.bDrawFov && localPawn) {
            uintptr_t cameraServices = mem.Read<uintptr_t>(localPawn + offsets::csPawn::m_pCameraServices);
            if (cameraServices) {
                float gameFov = static_cast<float>(mem.Read<uint32_t>(cameraServices + offsets::cameraServices::m_iFOV));
                if (gameFov < 1.0f) gameFov = 90.0f;
                float aimFov = config.aimFov;
                if (aimFov > 0.0f && aimFov < 179.5f) {
                    float halfGameRad = (gameFov * 0.5f) * (M_PI_F / 180.0f);
                    float aimRad = aimFov * (M_PI_F / 180.0f);
                    float radiusH = (g_width * 0.5f) * tanf(aimRad) / tanf(halfGameRad);
                    float verticalFov = 2.0f * atanf((g_height / (float)g_width) * tanf(halfGameRad));
                    float halfVertRad = verticalFov * 0.5f;
                    float radiusV = (g_height * 0.5f) * tanf(aimRad) / tanf(halfVertRad);
                    const float maxRad = sqrtf(g_width * g_width + g_height * g_height) * 2.0f;
                    radiusH = std::clamp(radiusH, 1.0f, maxRad);
                    radiusV = std::clamp(radiusV, 1.0f, maxRad);

                    ImDrawList* draw = ImGui::GetBackgroundDrawList();
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        config.aimFovColor[0],
                        config.aimFovColor[1],
                        config.aimFovColor[2],
                        config.aimFovColor[3]
                    ));
                    float centerX = g_width * 0.5f;
                    float centerY = g_height * 0.5f;
                    const int segments = 64;
                    for (int i = 0; i < segments; ++i) {
                        float angle = (i / (float)segments) * 2.0f * M_PI_F;
                        float nextAngle = ((i + 1) / (float)segments) * 2.0f * M_PI_F;
                        ImVec2 p1(centerX + cosf(angle) * radiusH, centerY + sinf(angle) * radiusV);
                        ImVec2 p2(centerX + cosf(nextAngle) * radiusH, centerY + sinf(nextAngle) * radiusV);
                        draw->AddLine(p1, p2, col, 1.5f);
                    }
                }
            }
        }

        // ─── Present ──────────────────────────────────────────────────────
        ImGui::Render();
        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dContext->OMSetRenderTargets(1, &g_pRTV, nullptr);
        g_pd3dContext->ClearRenderTargetView(g_pRTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(0, 0);

        int currentFps = config.show_menu ? MENU_FPS : TARGET_FPS;
        WaitForNextFrame(frameStart, currentFps);
    }
}

// ─── Overlay::Create ──────────────────────────────────────────────────────
bool Overlay::Create() {
    g_gameWnd = FindWindowA(nullptr, xorstr_("Counter-Strike 2"));
    if (!g_gameWnd) return false;

    RECT gr;
    GetWindowRect(g_gameWnd, &gr);
    g_width = gr.right - gr.left;
    g_height = gr.bottom - gr.top;

    g_overlay = CreateCustomOverlay(GetModuleHandle(nullptr), gr.left, gr.top, g_width, g_height);
    if (!g_overlay) return false;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    if (!CreateDeviceD3D(g_overlay)) {
        CleanupDeviceD3D();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyCustomStyle();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(g_overlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    auto TryLoadFont = [&](const char* paths[], int count, float size, const ImFontConfig* cfg) -> ImFont* {
        for (int i = 0; i < count; i++) {
            if (GetFileAttributesA(paths[i]) != INVALID_FILE_ATTRIBUTES)
                return io.Fonts->AddFontFromFileTTF(paths[i], size, cfg);
        }
        return nullptr;
        };

    ImFontConfig fontCfg{};
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;

    const char* regularPaths[] = { "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\calibri.ttf", "C:\\Windows\\Fonts\\arial.ttf" };
    ImFont* mainFont = TryLoadFont(regularPaths, 3, 14.0f, &fontCfg);
    if (!mainFont) mainFont = io.Fonts->AddFontDefault();

    FOV::Start();

    // ─── STARTA CACHE-TRÅD ──────────────────────────────────────────────
    g_cacheRunning = true;
    g_cacheThread = std::thread(CacheThreadFunc);

    SetStreamproof(config.bStreamproof);

    if (config.show_menu) {
        LONG_PTR exStyle = GetWindowLongPtr(g_overlay, GWL_EXSTYLE);
        exStyle &= ~WS_EX_TRANSPARENT;
        exStyle &= ~WS_EX_NOACTIVATE;
        SetWindowLongPtrW(g_overlay, GWL_EXSTYLE, exStyle);
        UpdateMenuClipCursor(true);
        FocusOverlayWindow();
    }

    return true;
}

// ─── Overlay::Destroy ─────────────────────────────────────────────────────
void Overlay::Destroy() {
    FOV::Stop();
    uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
    if (localPawn) FOV::Reset(localPawn);

    // ─── STOPPA CACHE-TRÅD ──────────────────────────────────────────────
    g_cacheRunning = false;
    if (g_cacheThread.joinable()) g_cacheThread.join();

    Radar::Cleanup();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    if (g_overlay) DestroyWindow(g_overlay);
    mem.Cleanup();
}

bool Overlay::IsRunning() { return g_running; }