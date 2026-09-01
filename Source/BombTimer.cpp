#include "BombTimer.h"
#include "Config.h"
#include "Memory.h"
#include "Offsets.h"
#include "SDK.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <cstdio>
#include <algorithm>
#include <d3d11.h>
#include <stb_image.h>

extern Memory mem;
extern ID3D11Device* g_pd3dDevice;

// ---- Texture creation ----
ImTextureID CreateTextureFromRGBA(int width, int height, const unsigned char* data) {
    if (!g_pd3dDevice) return 0;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;
    initData.SysMemPitch = width * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex)) || !tex)
        return 0;

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv)) || !srv) {
        tex->Release();
        return 0;
    }
    tex->Release();
    return (ImTextureID)srv;
}

namespace BombTimer {

    static uintptr_t g_plantedC4 = 0;
    static float g_timeRemaining = 0.0f;
    static bool g_isActive = false;
    static bool g_isDefusing = false;
    static bool g_isDefused = false;
    static float g_defuseProgress = 0.0f;
    static float g_totalTimerLength = 40.0f;
    static std::string g_defuserName;
    static char g_siteChar = 'A';

    static Vector3 g_bombOrigin;
    static float g_distanceToBomb = 0.0f;

    static ImTextureID g_bombTexture = 0;
    static int g_texWidth = 0, g_texHeight = 0;
    static ImFont* g_bigFont = nullptr;
    static ImFont* g_siteFont = nullptr;

    static ImVec2 g_widgetPos = ImVec2(0, 0);
    static float g_defuseRemaining = 0.0f;

    static uintptr_t GetEntityFromHandle(uint32_t handle) {
        if (!handle || handle == 0xFFFFFFFF) return 0;
        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) return 0;
        int index = (handle & 0x7FFF) >> 9;
        int entry = (handle & 0x7FFF) & 0x1FF;
        uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * index + 16));
        if (!listEntry) return 0;
        return mem.Read<uintptr_t>(listEntry + 112 * entry);
    }

    static void LoadBombTexture() {
        if (g_bombTexture) return;
        const char* path = "C:\\Velocity\\assets\\pictures\\bomb.png";
        int channels;
        unsigned char* data = stbi_load(path, &g_texWidth, &g_texHeight, &channels, 4);
        if (!data) {
            g_bombTexture = 0;
            return;
        }
        g_bombTexture = CreateTextureFromRGBA(g_texWidth, g_texHeight, data);
        stbi_image_free(data);
    }

    static float GetCurrentTime() {
        uintptr_t globalVars = mem.Read<uintptr_t>(mem.client + offsets::client::dwGlobalVars);
        if (!globalVars) return 0.0f;
        float time = mem.Read<float>(globalVars + 0x28);
        if (time > 10.0f && time < 10000.0f) return time;
        time = mem.Read<float>(globalVars + 0x2C);
        if (time > 10.0f && time < 10000.0f) return time;
        time = mem.Read<float>(globalVars + 0x30);
        if (time > 10.0f && time < 10000.0f) return time;
        return 0.0f;
    }

    void Update() {
        g_plantedC4 = mem.Read<uintptr_t>(mem.client + offsets::client::dwPlantedC4);

        if (!g_plantedC4) {
            g_isDefusing = false;
            g_isDefused = false;
            g_defuserName.clear();
            g_defuseRemaining = 0.0f;
            g_isActive = false;
            g_siteChar = '?';
            return;
        }

        // Read bombsite
        int site = mem.Read<int>(g_plantedC4 + offsets::C_PlantedC4::m_nBombSite);
        if (site == 0) g_siteChar = 'A';
        else if (site == 1) g_siteChar = 'B';
        else g_siteChar = '?';

        g_isDefused = mem.Read<bool>(g_plantedC4 + offsets::C_PlantedC4::m_bBombDefused);
        if (g_isDefused) {
            g_isActive = false;
            g_isDefusing = false;
            g_defuserName.clear();
            g_defuseRemaining = 0.0f;
            return;
        }

        g_isActive = mem.Read<bool>(g_plantedC4 + offsets::C_PlantedC4::m_bBombTicking);
        if (!g_isActive) {
            g_isDefusing = false;
            g_defuserName.clear();
            g_defuseRemaining = 0.0f;
            return;
        }

        float c4Blow = mem.Read<float>(g_plantedC4 + offsets::C_PlantedC4::m_flC4Blow);
        g_totalTimerLength = mem.Read<float>(g_plantedC4 + offsets::C_PlantedC4::m_flTimerLength);
        if (g_totalTimerLength <= 0.0f) g_totalTimerLength = 40.0f;

        float curTime = GetCurrentTime();
        g_timeRemaining = c4Blow - curTime;
        if (g_timeRemaining < -1.0f) {
            g_isActive = false;
            g_isDefusing = false;
            g_defuserName.clear();
            g_defuseRemaining = 0.0f;
            return;
        }
        if (g_timeRemaining < 0.0f) g_timeRemaining = 0.0f;

        // ---- Bomb origin ----
        uintptr_t bombScene = mem.Read<uintptr_t>(g_plantedC4 + offsets::entity::m_pGameSceneNode);
        if (bombScene)
            g_bombOrigin = mem.Read<Vector3>(bombScene + offsets::sceneNode::m_vecAbsOrigin);
        else
            g_bombOrigin = { 0,0,0 };

        // ---- Local player origin ----
        uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
        Vector3 localOrigin = { 0,0,0 };
        if (localPawn) {
            uintptr_t localScene = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
            if (localScene)
                localOrigin = mem.Read<Vector3>(localScene + offsets::sceneNode::m_vecAbsOrigin);
        }
        g_distanceToBomb = g_bombOrigin.Distance(localOrigin);

        // ---- Defuse detection using m_bBeingDefused ----
        bool isBeingDefused = mem.Read<bool>(g_plantedC4 + offsets::C_PlantedC4::m_bBeingDefused);
        if (isBeingDefused) {
            float defuseLen = mem.Read<float>(g_plantedC4 + offsets::C_PlantedC4::m_flDefuseLength);
            float defuseCountDown = mem.Read<float>(g_plantedC4 + offsets::C_PlantedC4::m_flDefuseCountDown);
            if (defuseLen > 0.0f) {
                float remaining = defuseCountDown - curTime;
                if (remaining < 0.0f) remaining = 0.0f;
                g_defuseRemaining = remaining;
                g_defuseProgress = remaining / defuseLen;
                if (g_defuseProgress < 0.0f) g_defuseProgress = 0.0f;
                if (g_defuseProgress > 1.0f) g_defuseProgress = 1.0f;
                g_isDefusing = true;

                // Get defuser name
                uint32_t defuserCtrlHandle = mem.Read<uint32_t>(g_plantedC4 + offsets::C_PlantedC4::m_pBombDefuser);
                if (defuserCtrlHandle && defuserCtrlHandle != 0xFFFFFFFF) {
                    uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
                    if (entityList) {
                        uintptr_t ctrlEntry = mem.Read<uintptr_t>(entityList + (8 * ((defuserCtrlHandle & 0x7FFF) >> 9) + 16));
                        if (ctrlEntry) {
                            uintptr_t controller = mem.Read<uintptr_t>(ctrlEntry + 112 * ((defuserCtrlHandle & 0x7FFF) & 0x1FF));
                            if (controller) {
                                char nameBuf[128] = {};
                                mem.ReadRaw(controller + offsets::controller::m_iszPlayerName, nameBuf, sizeof(nameBuf) - 1);
                                g_defuserName = nameBuf;
                            }
                        }
                    }
                }
            }
        }
        else {
            // Defusing stopped or never started
            g_isDefusing = false;
            g_defuserName.clear();
            g_defuseRemaining = 0.0f;
            g_defuseProgress = 0.0f;
        }
    }

    bool IsActive() { return g_isActive && !g_isDefused; }
    float GetTimeRemaining() { return g_timeRemaining; }

    std::string GetTimeString() {
        if (!IsActive()) return "";
        int seconds = static_cast<int>(std::ceil(g_timeRemaining));
        int mins = seconds / 60;
        int secs = seconds % 60;
        char buf[16];
        if (mins > 0) snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        else snprintf(buf, sizeof(buf), "%d", secs);
        return std::string(buf);
    }

    bool IsDefusing() { return g_isDefusing; }
    float GetDefuseProgress() { return g_defuseProgress; }
    std::string GetDefuserName() { return g_defuserName; }

    void Draw() {
        if (!g_plantedC4) return;

        static bool textureLoaded = false;
        if (!textureLoaded) {
            LoadBombTexture();
            textureLoaded = true;
        }

        static bool fontsLoaded = false;
        if (!fontsLoaded) {
            ImGuiIO& io = ImGui::GetIO();
            ImFontConfig cfg;
            cfg.OversampleH = 3;
            cfg.OversampleV = 2;
            cfg.SizePixels = 28.0f;
            g_bigFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 28.0f, &cfg);
            if (!g_bigFont) g_bigFont = io.Fonts->AddFontDefault(&cfg);

            ImFontConfig cfgSite;
            cfgSite.OversampleH = 3;
            cfgSite.OversampleV = 2;
            cfgSite.SizePixels = 40.0f;
            g_siteFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 40.0f, &cfgSite);
            if (!g_siteFont) g_siteFont = io.Fonts->AddFontDefault(&cfgSite);

            io.Fonts->Build();
            fontsLoaded = true;
        }

        const float iconRadius = 32.0f;
        const float widgetWidth = 200.0f;
        const float widgetHeight = 80.0f;

        if (g_widgetPos.x == 0.0f && g_widgetPos.y == 0.0f) {
            ImVec2 screenSize = ImGui::GetIO().DisplaySize;
            g_widgetPos = ImVec2(screenSize.x * 0.5f - widgetWidth * 0.5f,
                screenSize.y - 120.0f - widgetHeight * 0.5f);
        }

        ImGui::SetNextWindowPos(g_widgetPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(widgetWidth, widgetHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin("BombTimer", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float left = cursor.x;
            float top = cursor.y;

            ImGui::InvisibleButton("drag", ImVec2(widgetWidth, widgetHeight));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                g_widgetPos.x += ImGui::GetIO().MouseDelta.x;
                g_widgetPos.y += ImGui::GetIO().MouseDelta.y;
                ImGui::SetWindowPos(g_widgetPos);
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(ImVec2(left, top), ImVec2(left + widgetWidth, top + widgetHeight),
                IM_COL32(0, 0, 0, 180), 8.0f);
            draw->AddRect(ImVec2(left, top), ImVec2(left + widgetWidth, top + widgetHeight),
                IM_COL32(255, 255, 255, 60), 8.0f, 0, 1.0f);

            float iconX = left + widgetWidth - iconRadius - 16.0f;
            float iconY = top + widgetHeight * 0.5f + 1.0f;

            draw->AddCircleFilled(ImVec2(iconX, iconY), iconRadius, IM_COL32(40, 40, 45, 200));
            draw->AddCircle(ImVec2(iconX, iconY), iconRadius, IM_COL32(200, 200, 200, 80), 32, 1.5f);

            // Inner ring (bomb timer)
            if (g_plantedC4 && g_isActive) {
                float progress = g_timeRemaining / g_totalTimerLength;
                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;
                ImU32 ringColor;
                if (progress > 0.5f) {
                    float t = (progress - 0.5f) * 2.0f;
                    ringColor = IM_COL32(static_cast<int>(255 * (1.0f - t)), 255, 0, 255);
                }
                else {
                    float t = progress * 2.0f;
                    ringColor = IM_COL32(255, static_cast<int>(255 * t), 0, 255);
                }
                const int segments = 48;
                float startAngle = -M_PI_F * 0.5f;
                float endAngle = startAngle + (1.0f - progress) * 2.0f * M_PI_F;
                for (int i = 0; i < segments; i++) {
                    float a1 = startAngle + (i / (float)segments) * (endAngle - startAngle);
                    float a2 = startAngle + ((i + 1) / (float)segments) * (endAngle - startAngle);
                    ImVec2 p1(iconX + cosf(a1) * iconRadius, iconY + sinf(a1) * iconRadius);
                    ImVec2 p2(iconX + cosf(a2) * iconRadius, iconY + sinf(a2) * iconRadius);
                    draw->AddLine(p1, p2, ringColor, 3.0f);
                }
            }

            // Outer ring (defuse progress)
            if (g_isDefusing) {
                float defuseProgress = GetDefuseProgress();
                ImU32 defuseColor = IM_COL32(0, 200, 255, 200);
                float outerRad = iconRadius + 4.0f;
                const int segments = 48;
                float startAngle = -M_PI_F * 0.5f;
                float endAngle = startAngle + defuseProgress * 2.0f * M_PI_F;
                for (int i = 0; i < segments; i++) {
                    float a1 = startAngle + (i / (float)segments) * (endAngle - startAngle);
                    float a2 = startAngle + ((i + 1) / (float)segments) * (endAngle - startAngle);
                    ImVec2 p1(iconX + cosf(a1) * outerRad, iconY + sinf(a1) * outerRad);
                    ImVec2 p2(iconX + cosf(a2) * outerRad, iconY + sinf(a2) * outerRad);
                    draw->AddLine(p1, p2, defuseColor, 2.5f);
                }
                draw->AddText(ImVec2(iconX - 6, iconY - 8), IM_COL32(0, 200, 255, 255), "D");
            }

            // Bomb texture
            float iconDrawY = iconY + 16.0f;
            float iconDrawX = iconX - 2.0f;
            if (g_bombTexture && g_texWidth > 0 && g_texHeight > 0) {
                float maxDim = iconRadius * 3.25f;
                float scale = (std::min)(maxDim / g_texWidth, maxDim / g_texHeight);
                float drawW = g_texWidth * scale;
                float drawH = g_texHeight * scale;
                ImVec2 topLeft(iconDrawX - drawW * 0.5f, iconDrawY - drawH * 0.5f);
                ImVec2 bottomRight(iconDrawX + drawW * 0.5f, iconDrawY + drawH * 0.5f);
                draw->AddImage(g_bombTexture, topLeft, bottomRight);
            }
            else {
                draw->AddText(ImVec2(iconDrawX - 20, iconDrawY - 20),
                    IM_COL32(255, 200, 100, 255), "B");
            }

            // Site letter
            float siteX = left + 12.0f;
            float siteY = top + (widgetHeight - 40.0f) * 0.5f;
            char siteBuf[2] = { g_siteChar, 0 };
            draw->AddText(g_siteFont, 40.0f, ImVec2(siteX, siteY), IM_COL32(255, 255, 255, 255), siteBuf);

            // Timer text
            float timerX = iconX - iconRadius - 50.0f;
            float timerY = iconY - 14.0f;

            if (g_isDefusing) {
                int defuseSeconds = static_cast<int>(std::ceil(g_defuseRemaining));
                if (defuseSeconds < 0) defuseSeconds = 0;
                char defuseBuf[16];
                snprintf(defuseBuf, sizeof(defuseBuf), "%d", defuseSeconds);
                ImGui::PushFont(g_bigFont);
                draw->AddText(ImVec2(timerX + 1, timerY + 1), IM_COL32(0, 0, 0, 200), defuseBuf);
                draw->AddText(ImVec2(timerX, timerY), IM_COL32(0, 200, 255, 255), defuseBuf);
                ImGui::PopFont();
            }
            else if (g_plantedC4 && g_isActive) {
                std::string timerText = GetTimeString();
                ImU32 timerColor = (g_timeRemaining <= 5.0f) ? IM_COL32(255, 50, 50, 255)
                    : (g_timeRemaining <= 10.0f) ? IM_COL32(255, 200, 50, 255)
                    : IM_COL32(255, 255, 255, 255);
                ImGui::PushFont(g_bigFont);
                draw->AddText(ImVec2(timerX + 1, timerY + 1), IM_COL32(0, 0, 0, 200), timerText.c_str());
                draw->AddText(ImVec2(timerX, timerY), timerColor, timerText.c_str());
                ImGui::PopFont();
            }

            // Defuser name
            if (g_isDefusing && !g_defuserName.empty()) {
                std::string defText = "Defusing: " + g_defuserName;
                float defX = timerX - 10.0f;
                float defY = iconY + 18.0f;
                draw->AddText(ImVec2(defX, defY), IM_COL32(0, 200, 255, 200), defText.c_str());
            }

            ImGui::End();
        }
        ImGui::PopStyleVar();
    }
}