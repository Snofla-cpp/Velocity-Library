#include "Menu.h"
#include "Config.h"
#include "Overlay.h"
#include <imgui.h>
#include <Memory.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <shellapi.h>   // for ShellExecute

extern bool g_unload_requested;

// ---- Key listener ----
static int* g_listeningKey = nullptr;
static bool g_listeningWaitingForRelease = false;   // <-- NEW

static ID3D11ShaderResourceView* LoadTexture(const char* path, int* outWidth = nullptr, int* outHeight = nullptr) {
    int width, height, channels;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        return nullptr;
    }
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
    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = data;
    subResource.SysMemPitch = desc.Width * 4;
    ID3D11Texture2D* pTexture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
    if (SUCCEEDED(hr)) {
        g_pd3dDevice->CreateShaderResourceView(pTexture, nullptr, &textureView);
        pTexture->Release();
    }
    stbi_image_free(data);
    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;
    return textureView;
}

static void StyleCheckbox(bool state) {
    const ImVec4 greyBg = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    const ImVec4 greyHovered = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    const ImVec4 themeColor = ImVec4(0.9686f, 0.7961f, 0.8196f, 1.0f);
    const ImVec4 themeHovered = ImVec4(0.99f, 0.85f, 0.87f, 1.0f);
    const ImVec4 themeActive = ImVec4(0.95f, 0.78f, 0.80f, 1.0f);
    if (state) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, themeColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, themeHovered);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, themeActive);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, greyBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, greyHovered);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, greyHovered);
    }
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
}

// ---- Helper: get key name ----
static const char* GetKeyName(int vk) {
    switch (vk) {
    case 0x01: return "M1";   case 0x02: return "M2";
    case 0x04: return "M3";   case 0x05: return "M4";
    case 0x06: return "M5";   case 0x10: return "SHFT";
    case 0x11: return "CTRL"; case 0x12: return "ALT";
    case 0x14: return "CAPS"; case 0x20: return "SPC";
    case 0x09: return "TAB";
    case VK_F1:  return "F1";  case VK_F2:  return "F2";
    case VK_F3:  return "F3";  case VK_F4:  return "F4";
    case VK_F5:  return "F5";  case VK_F6:  return "F6";
    case VK_F7:  return "F7";  case VK_F8:  return "F8";
    case VK_F9:  return "F9";  case VK_F10: return "F10";
    case VK_F11: return "F11"; case VK_F12: return "F12";
    default:
        if (vk >= 0x30 && vk <= 0x39) { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
        if (vk >= 0x41 && vk <= 0x5A) { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
        static char hex[8]; snprintf(hex, sizeof(hex), "0x%02X", vk); return hex;
    }
}

// ---- Key bind with listener (no popup) ----
static void KeyBind(const char* id, const char* label, int* key) {
    bool listening = (g_listeningKey == key);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float avail = ImGui::GetContentRegionAvail().x;

    // ---- Label with scaling (same as other feature texts) ----
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s", label);
    ImGui::SetWindowFontScale(1.0f);

    // Position button on the right side
    float btnW = 72.0f;
    ImGui::SameLine(avail - btnW - 5.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f); // fine‑tune vertical align

    char btnLabel[64];
    if (listening)
        snprintf(btnLabel, sizeof(btnLabel), "...##%s", id);
    else
        snprintf(btnLabel, sizeof(btnLabel), "[%s]##%s", GetKeyName(*key), id);

    // ---- Button colors ----
    ImVec4 cardBg = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    ImVec4 cardHovered = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    ImVec4 cardActive = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    ImVec4 listeningBg = ImVec4(0.02f, 0.14f, 0.22f, 1.0f);
    ImVec4 listeningHovered = ImVec4(0.04f, 0.18f, 0.28f, 1.0f);
    ImVec4 listeningActive = ImVec4(0.06f, 0.22f, 0.34f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, listening ? listeningBg : cardBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, listening ? listeningHovered : cardHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, listening ? listeningActive : cardActive);
    ImVec4 textCol = listening ? ImVec4(0.0f, 0.88f, 1.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(btnLabel, ImVec2(btnW, 20))) {
        if (!listening) {
            g_listeningKey = key;
            g_listeningWaitingForRelease = true;   // wait for all keys to be released
        }
        else {
            g_listeningKey = nullptr;
            g_listeningWaitingForRelease = false;
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // Border
    ImU32 borderCol = listening ? IM_COL32(0, 200, 255, 255) : IM_COL32(247, 203, 209, 150);
    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(bMin, bMax, borderCol, 6.0f, 0, 1.2f);

    // Reserve space for next item
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetFrameHeight() + 4.0f);
    ImGui::Dummy(ImVec2(avail, 0.0f));

    // ---- Key listener logic ----
    if (g_listeningKey == key) {
        // 1) Wait for all keys to be released (so the click itself isn't captured)
        if (g_listeningWaitingForRelease) {
            bool anyKeyDown = false;
            // Mouse buttons (2‑6) – skip left click (1)
            for (int m = 2; m <= 6; m++) {
                if (GetAsyncKeyState(m) & 0x8000) {
                    anyKeyDown = true;
                    break;
                }
            }
            // Keyboard keys (0x08 to 0xFE), skipping Insert and End
            if (!anyKeyDown) {
                for (int k = 0x08; k <= 0xFE; k++) {
                    if (k == VK_INSERT || k == VK_END) continue;
                    if (GetAsyncKeyState(k) & 0x8000) {
                        anyKeyDown = true;
                        break;
                    }
                }
            }
            if (anyKeyDown)
                return; // still holding something – wait
            g_listeningWaitingForRelease = false; // all released
        }

        // 2) Now accept a new key press (using the "pressed since last call" flag)
        // Mouse buttons (2‑6)
        for (int m = 2; m <= 6; m++) {
            if (GetAsyncKeyState(m) & 1) {
                *key = m;
                g_listeningKey = nullptr;
                g_listeningWaitingForRelease = false;
                return;
            }
        }
        // Keyboard keys (0x08 to 0xFE), skipping Insert and End
        for (int k = 0x08; k <= 0xFE; k++) {
            if (k == VK_INSERT || k == VK_END) continue;
            if (k == VK_ESCAPE) {
                if (GetAsyncKeyState(k) & 1) {
                    // Cancel listening
                    g_listeningKey = nullptr;
                    g_listeningWaitingForRelease = false;
                    return;
                }
                continue;
            }
            if (GetAsyncKeyState(k) & 1) {
                *key = k;
                g_listeningKey = nullptr;
                g_listeningWaitingForRelease = false;
                Sleep(50); // debounce
                return;
            }
        }
    }
}

static void SliderFloatStyled(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f") {
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::SliderFloat(label, v, v_min, v_max, "");
    ImGui::PopStyleColor(2);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    float rounding = ImGui::GetStyle().FrameRounding;

    ImU32 greyBg = IM_COL32(45, 45, 50, 255);
    ImU32 pinkFill = IM_COL32(247, 203, 209, 255);

    draw->AddRectFilled(rect_min, rect_max, greyBg, rounding);

    float width = rect_max.x - rect_min.x;
    float fill_x = rect_min.x + ((*v - v_min) / (v_max - v_min)) * width;
    fill_x = std::clamp(fill_x, rect_min.x, rect_max.x);

    if (fill_x > rect_min.x) {
        ImVec2 fillMin = rect_min;
        ImVec2 fillMax = ImVec2(fill_x, rect_max.y);
        draw->AddRectFilled(fillMin, fillMax, pinkFill, rounding);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), format, *v);
    ImVec2 textSize = ImGui::CalcTextSize(buf);
    float textX = rect_min.x + (rect_max.x - rect_min.x - textSize.x) * 0.5f;
    float textY = rect_min.y + (rect_max.y - rect_min.y - textSize.y) * 0.5f;
    ImU32 textCol = IM_COL32(140, 140, 140, 255);
    draw->AddText(ImVec2(textX, textY), textCol, buf);
}

static void SliderIntStyled(const char* label, int* v, int v_min, int v_max, const char* format = "%d") {
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::SliderInt(label, v, v_min, v_max, "");
    ImGui::PopStyleColor(2);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    float rounding = ImGui::GetStyle().FrameRounding;

    ImU32 greyBg = IM_COL32(45, 45, 50, 255);
    ImU32 pinkFill = IM_COL32(247, 203, 209, 255);

    draw->AddRectFilled(rect_min, rect_max, greyBg, rounding);

    float width = rect_max.x - rect_min.x;
    float fill_x = rect_min.x + ((*v - v_min) / (float)(v_max - v_min)) * width;
    fill_x = std::clamp(fill_x, rect_min.x, rect_max.x);

    if (fill_x > rect_min.x) {
        ImVec2 fillMin = rect_min;
        ImVec2 fillMax = ImVec2(fill_x, rect_max.y);
        draw->AddRectFilled(fillMin, fillMax, pinkFill, rounding);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), format, *v);
    ImVec2 textSize = ImGui::CalcTextSize(buf);
    float textX = rect_min.x + (rect_max.x - rect_min.x - textSize.x) * 0.5f;
    float textY = rect_min.y + (rect_max.y - rect_min.y - textSize.y) * 0.5f;
    ImU32 textCol = IM_COL32(140, 140, 140, 255);
    draw->AddText(ImVec2(textX, textY), textCol, buf);
}

void menu::Render() {
    if (!config.show_menu) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##MainBox", nullptr, flags);

    static ImFont* symbolFont = nullptr;
    if (!symbolFont) {
        ImGuiIO& io = ImGui::GetIO();
        const char* fontPaths[] = { "C:\\Windows\\Fonts\\seguisym.ttf", "C:\\Windows\\Fonts\\seguiemj.ttf", "C:\\Windows\\Fonts\\segoeui.ttf" };
        for (const char* path : fontPaths) {
            symbolFont = io.Fonts->AddFontFromFileTTF(path, 30.0f);
            if (symbolFont) break;
        }
        if (!symbolFont) symbolFont = io.Fonts->AddFontDefault();
        io.Fonts->Build();
    }

    static ID3D11ShaderResourceView* logoTexture = nullptr;
    static int logoWidth = 0, logoHeight = 0;
    if (!logoTexture) logoTexture = LoadTexture("C:\\Velocity\\assets\\pictures\\velocity.png", &logoWidth, &logoHeight);

    const float logoHeightPx = 72.0f;
    float logoWidthPx = logoHeightPx;
    if (logoTexture && logoWidth > 0 && logoHeight > 0) {
        float aspect = static_cast<float>(logoWidth) / static_cast<float>(logoHeight);
        logoWidthPx = logoHeightPx * aspect;
    }
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 winPos = ImGui::GetWindowPos();
    if (logoTexture && logoWidth > 0 && logoHeight > 0) {
        ImGui::SetCursorPosX(10);
        ImGui::SetCursorPosY(8);
        ImGui::Image(reinterpret_cast<void*>(logoTexture), ImVec2(logoWidthPx, logoHeightPx));
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    float lineX = 10 + logoWidthPx + 10;
    float lineY1 = 8.0f, lineY2 = windowSize.y - 10.0f;
    draw->AddLine(ImVec2(winPos.x + lineX, winPos.y + lineY1),
        ImVec2(winPos.x + lineX, winPos.y + lineY2),
        IM_COL32(247, 203, 209, 80), 1.0f);

    static const char* categorySymbols[] = { "", "", "", "" };
    const float iconSize = 64.0f, spacing = 12.0f;
    float startY = 8 + logoHeightPx + 10;
    static int currentCategory = 0;
    if (symbolFont) ImGui::PushFont(symbolFont);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    const ImVec4 themeColor = ImVec4(0.9686f, 0.7961f, 0.8196f, 1.0f);
    const ImVec4 whiteColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const float offsetX = -2.0f;
    for (int i = 0; i < 4; i++) {
        ImGui::PushID(i);
        float panelWidth = lineX - 10;
        float centerX = 10 + (panelWidth - iconSize) * 0.5f + offsetX;
        ImGui::SetCursorPosX(centerX);
        ImGui::SetCursorPosY(startY + i * (iconSize + spacing));
        bool isActive = (i == currentCategory);
        ImGui::PushStyleColor(ImGuiCol_Text, isActive ? whiteColor : themeColor);
        if (ImGui::Button(categorySymbols[i], ImVec2(iconSize, iconSize))) currentCategory = i;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (symbolFont) ImGui::PopFont();

    ImGui::SetCursorPosX(lineX + 10);
    ImGui::SetCursorPosY(8);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    float contentHeight = (windowSize.y - 10.0f) - 8.0f;
    ImGui::BeginChild("##Content", ImVec2(windowSize.x - (lineX + 10) - 10, contentHeight),
        false, ImGuiWindowFlags_NoScrollbar);

    // ============================================================
    // AIM
    // ============================================================
    if (currentCategory == 0) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float cardWidth = (avail.x - 10.0f) * 0.5f;
        float cardHeight = avail.y;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));

        // ---- Aimbot Card (left) ----
        ImGui::BeginChild("##AimbotCard", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);

        // ---- Enable Aimbot ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Enable Aimbot");
        ImGui::SetWindowFontScale(1.0f);
        float rightEdge = ImGui::GetContentRegionMax().x;
        float checkboxSize = 20.0f;
        float checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bAimbot);
        ImGui::Checkbox("##AimbotEnable", &config.bAimbot);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Aimbot Mode Dropdown ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Mode");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(70.0f);
        ImGui::PushItemWidth(120.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
        ImVec4 themeCol2 = ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f);
        ImVec4 cardBg = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, themeCol2);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
        const char* aimModes[] = { "Always", "Hold", "Toggle" };
        if (ImGui::Combo("##AimMode", &config.aimMode, aimModes, IM_ARRAYSIZE(aimModes))) {}
        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Aimbot Key (listener) ----
        KeyBind("aimkey", "Key", &config.aimKey);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Hitbox Selection (multiselect dropdown) ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Hitboxes");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(70.0f);

        static const char* hitboxNames[] = { "Head", "Neck", "Chest", "Pelvis" };
        std::string label;
        for (int i = 0; i < 4; i++) {
            if (config.hitboxMask & (1 << i)) {
                if (!label.empty()) label += ", ";
                label += hitboxNames[i];
            }
        }
        if (label.empty()) label = "None";

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.28f, 0.35f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.6f));

        if (ImGui::Button(label.c_str(), ImVec2(120, 20))) {
            ImGui::OpenPopup("HitboxPopup");
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);

        if (ImGui::BeginPopup("HitboxPopup")) {
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9686f, 0.7961f, 0.8196f, 1.0f));
            ImGui::Text("Select hitboxes:");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.6f));
            ImGui::Separator();
            ImGui::PopStyleColor();

            for (int i = 0; i < 4; i++) {
                bool enabled = (config.hitboxMask & (1 << i)) != 0;
                if (ImGui::Checkbox(hitboxNames[i], &enabled)) {
                    if (enabled)
                        config.hitboxMask |= (1 << i);
                    else
                        config.hitboxMask &= ~(1 << i);
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.6f));
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
            if (ImGui::Button("OK", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- FOV Slider ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("FOV");
        ImGui::SetWindowFontScale(1.0f);
        float sliderWidth = ImGui::GetContentRegionAvail().x - 5.0f;
        ImGui::PushItemWidth(sliderWidth);
        SliderFloatStyled("##AimFov", &config.aimFov, 0.0f, 180.0f, "%.1f");
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Draw FOV Checkbox + Color Picker ----
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Draw FOV");
            ImGui::SetWindowFontScale(1.0f);
            float fovRightEdge = ImGui::GetContentRegionMax().x;
            float fovCheckboxSize = 20.0f;
            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float fovColorPickerX = fovRightEdge - fovCheckboxSize - gap - colourButtonWidth - 5.0f;
            float fovCheckboxX = fovRightEdge - fovCheckboxSize - 5.0f;

            ImGui::SameLine();
            ImGui::SetCursorPosX(fovColorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##FovColorPicker",
                ImVec4(config.aimFovColor[0], config.aimFovColor[1], config.aimFovColor[2], config.aimFovColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##FovColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##FovColorPopup")) {
                ImGui::ColorPicker4("##FovColorPicker", config.aimFovColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(fovCheckboxX);
            StyleCheckbox(config.bDrawFov);
            ImGui::Checkbox("##DrawFovToggle", &config.bDrawFov);
            ImGui::PopStyleColor(4);
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        // ---- Smooth Slider ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Smooth");
        ImGui::SetWindowFontScale(1.0f);
        sliderWidth = ImGui::GetContentRegionAvail().x - 5.0f;
        ImGui::PushItemWidth(sliderWidth);
        SliderFloatStyled("##AimSmooth", &config.aimSmooth, 0.0f, 100.0f, "%.1f");
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Randomize Smooth ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Randomize Smooth");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bAimSmoothRandom);
        ImGui::Checkbox("##AimSmoothRandom", &config.bAimSmoothRandom);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (config.bAimSmoothRandom) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Min");
            ImGui::SetWindowFontScale(1.0f);
            float availWidth = ImGui::GetContentRegionAvail().x - 5.0f;
            float halfWidth = (availWidth - 10.0f) * 0.5f;
            ImGui::PushItemWidth(halfWidth);
            SliderFloatStyled("##AimSmoothMin", &config.aimSmoothMin, 0.0f, 100.0f, "%.1f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Max");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushItemWidth(halfWidth);
            SliderFloatStyled("##AimSmoothMax", &config.aimSmoothMax, 0.0f, 100.0f, "%.1f");
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        // ---- Aim Jitter ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Aim Jitter");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bAimJitter);
        ImGui::Checkbox("##AimJitter", &config.bAimJitter);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (config.bAimJitter) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Amount");
            ImGui::SetWindowFontScale(1.0f);
            sliderWidth = ImGui::GetContentRegionAvail().x - 5.0f;
            ImGui::PushItemWidth(sliderWidth);
            SliderFloatStyled("##AimJitterAmount", &config.aimJitterAmount, 0.0f, 2.0f, "%.2f°");
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        // ---- Visible Check ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Visible Check");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bVisibleCheck);
        ImGui::Checkbox("##VisibleCheck", &config.bVisibleCheck);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::EndChild(); // AimbotCard

        // ---- Triggerbot Card (right) ----
        ImGui::SameLine();
        ImGui::BeginChild("##TriggerbotCard", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);

        // ---- Enable Triggerbot ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Enable Triggerbot");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        checkboxSize = 20.0f;
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bTriggerbot);
        ImGui::Checkbox("##TriggerbotEnable", &config.bTriggerbot);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Triggerbot Mode Dropdown ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Mode");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(70.0f);
        ImGui::PushItemWidth(120.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, themeCol2);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, cardBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
        const char* triggerModes[] = { "Always", "Hold", "Toggle" };
        if (ImGui::Combo("##TriggerMode", &config.triggerMode, triggerModes, IM_ARRAYSIZE(triggerModes))) {}
        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Triggerbot Key (listener) ----
        KeyBind("trigkey", "Key", &config.triggerKey);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Reaction Time Slider ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Reaction Time (ms)");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        sliderWidth = (rightEdge - 5.0f) - 10.0f;
        ImGui::PushItemWidth(sliderWidth);
        SliderIntStyled("##TriggerReactionTime", &config.triggerReactionTime, 0, 200, "%d ms");
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Randomize Reaction Time ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Randomize Reaction");
        ImGui::SetWindowFontScale(1.0f);
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bTriggerReactionRandom);
        ImGui::Checkbox("##TriggerReactionRandom", &config.bTriggerReactionRandom);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (config.bTriggerReactionRandom) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Min");
            ImGui::SetWindowFontScale(1.0f);
            float halfWidth = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;
            ImGui::PushItemWidth(halfWidth);
            SliderIntStyled("##TriggerReactionMin", &config.triggerReactionMin, 0, 200, "%d ms");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Max");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushItemWidth(halfWidth);
            SliderIntStyled("##TriggerReactionMax", &config.triggerReactionMax, 0, 200, "%d ms");
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        // ---- Shot Cooldown Slider ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Shot Cooldown (ms)");
        ImGui::SetWindowFontScale(1.0f);
        rightEdge = ImGui::GetContentRegionMax().x;
        sliderWidth = (rightEdge - 5.0f) - 10.0f;
        ImGui::PushItemWidth(sliderWidth);
        SliderIntStyled("##TriggerShotCooldown", &config.triggerShotCooldown, 0, 500, "%d ms");
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 8.0f));

        // ---- Randomize Cooldown ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Randomize Cooldown");
        ImGui::SetWindowFontScale(1.0f);
        checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bTriggerCooldownRandom);
        ImGui::Checkbox("##TriggerCooldownRandom", &config.bTriggerCooldownRandom);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        if (config.bTriggerCooldownRandom) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Min");
            ImGui::SetWindowFontScale(1.0f);
            float halfWidth = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;
            ImGui::PushItemWidth(halfWidth);
            SliderIntStyled("##TriggerCooldownMin", &config.triggerCooldownMin, 0, 500, "%d ms");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Max");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushItemWidth(halfWidth);
            SliderIntStyled("##TriggerCooldownMax", &config.triggerCooldownMax, 0, 500, "%d ms");
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        ImGui::EndChild(); // TriggerbotCard

        ImGui::PopStyleVar(); // WindowPadding
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    // ============================================================
    // VISUALS
    // ============================================================
    else if (currentCategory == 1) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float cardWidth = (avail.x - 10.0f) * 0.5f;
        float cardHeight = avail.y;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));

        // =========================================================
        // ESP CARD (left)
        // =========================================================
        ImGui::BeginChild("##ESPcard", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);

        // ---- Enable ESP ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Enable ESP");
        ImGui::SetWindowFontScale(1.0f);
        float rightEdge = ImGui::GetContentRegionMax().x;
        float checkboxSize = 20.0f;
        float checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bEsp);
        ImGui::Checkbox("##EnableESP", &config.bEsp);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Team Check ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Team Check");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bEspTeamCheck);
        ImGui::Checkbox("##TeamCheck", &config.bEspTeamCheck);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Box Row ----
        ImGui::PushID("BoxRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Box");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gearSize = 28.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;
            float gearX = colorPickerX - gap - gearSize;

            ImGui::SameLine();
            ImGui::SetCursorPosX(gearX);
            float textHeight = ImGui::GetTextLineHeight();
            float buttonYOffset = (textHeight - gearSize) * 0.5f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonYOffset);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (symbolFont) ImGui::PushFont(symbolFont);
            if (ImGui::Button("", ImVec2(gearSize, gearSize))) ImGui::OpenPopup("BoxSettingsPopup");
            if (symbolFont) ImGui::PopFont();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // Box Settings Popup
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            if (ImGui::BeginPopup("BoxSettingsPopup")) {
                const ImVec4 themeCol = ImVec4(0.9686f, 0.7961f, 0.8196f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, themeCol);
                ImGui::Text("Box Settings");
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(themeCol.x, themeCol.y, themeCol.z, 0.8f));
                ImGui::Separator();
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, themeCol);
                ImGui::Text("Type");
                ImGui::PopStyleColor();
                ImGui::SameLine(100.0f);
                ImGui::PushItemWidth(80.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
                const char* boxTypes[] = { "2D", "Corner" };
                if (ImGui::Combo("##BoxType", &config.boxType, boxTypes, IM_ARRAYSIZE(boxTypes))) {}
                ImGui::PopStyleColor(5);
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();

                ImGui::PushStyleColor(ImGuiCol_Text, themeCol);
                ImGui::Text("Thickness");
                ImGui::PopStyleColor();
                ImGui::SameLine(100.0f);
                ImGui::PushItemWidth(80.0f);
                SliderFloatStyled("##BoxThickness", &config.boxThickness, 0.5f, 3.0f, "%.1f");
                ImGui::PopItemWidth();

                ImGui::PushStyleColor(ImGuiCol_Text, themeCol);
                ImGui::Text("Fill");
                ImGui::PopStyleColor();
                ImGui::SameLine(100.0f);
                ImGui::Checkbox("##BoxFill", &config.bBoxFill);

                ImGui::PushStyleColor(ImGuiCol_Text, themeCol);
                ImGui::Text("Fill Color");
                ImGui::PopStyleColor();
                ImGui::SameLine(100.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(themeCol.x, themeCol.y, themeCol.z, 0.8f));
                if (ImGui::ColorButton("##BoxFillColorPicker",
                    ImVec4(config.espFillColor[0], config.espFillColor[1], config.espFillColor[2], config.espFillColor[3]),
                    ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder, ImVec2(40, 20))) {
                    ImGui::OpenPopup("##BoxFillColorPopup");
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
                if (ImGui::BeginPopup("##BoxFillColorPopup")) {
                    ImGui::ColorPicker4("##BoxFillColorPicker", config.espFillColor,
                        ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                    ImGui::EndPopup();
                }

                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // Outline color picker (main row)
            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##BoxColorPicker",
                ImVec4(config.espBoxColor[0], config.espBoxColor[1], config.espBoxColor[2], config.espBoxColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##BoxColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##BoxColorPopup")) {
                ImGui::ColorPicker4("##BoxColorPicker", config.espBoxColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspBox);
            ImGui::Checkbox("##BoxToggle", &config.bEspBox);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // BoxRow

        // ---- Skeleton Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("SkeletonRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Skeleton");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##SkeletonColorPicker",
                ImVec4(config.espSkeletonColor[0], config.espSkeletonColor[1], config.espSkeletonColor[2], config.espSkeletonColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##SkeletonColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##SkeletonColorPopup")) {
                ImGui::ColorPicker4("##SkeletonColorPicker", config.espSkeletonColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspSkeleton);
            ImGui::Checkbox("##SkeletonToggle", &config.bEspSkeleton);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // SkeletonRow

        // ---- Name Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("NameRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Name");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##NameColorPicker",
                ImVec4(config.espNameColor[0], config.espNameColor[1], config.espNameColor[2], config.espNameColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##NameColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##NameColorPopup")) {
                ImGui::ColorPicker4("##NameColorPicker", config.espNameColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspName);
            ImGui::Checkbox("##NameToggle", &config.bEspName);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // NameRow

        // ---- Health Bar Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("HealthBarRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Health Bar");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##HealthColorPicker",
                ImVec4(config.espHealthColor[0], config.espHealthColor[1], config.espHealthColor[2], config.espHealthColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##HealthColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##HealthColorPopup")) {
                ImGui::ColorPicker4("##HealthColorPicker", config.espHealthColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspHealthBar);
            ImGui::Checkbox("##HealthToggle", &config.bEspHealthBar);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // HealthBarRow

        // ---- Armor Bar Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("ArmorBarRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Armor Bar");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##ArmorColorPicker",
                ImVec4(config.espArmorColor[0], config.espArmorColor[1], config.espArmorColor[2], config.espArmorColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##ArmorColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##ArmorColorPopup")) {
                ImGui::ColorPicker4("##ArmorColorPicker", config.espArmorColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspArmorBar);
            ImGui::Checkbox("##ArmorToggle", &config.bEspArmorBar);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // ArmorBarRow

        // ---- Head Circle Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("HeadCircleRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Head Circle");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##HeadCircleColorPicker",
                ImVec4(config.espHeadCircleColor[0], config.espHeadCircleColor[1], config.espHeadCircleColor[2], config.espHeadCircleColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##HeadCircleColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##HeadCircleColorPopup")) {
                ImGui::ColorPicker4("##HeadCircleColorPicker", config.espHeadCircleColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspHeadCircle);
            ImGui::Checkbox("##HeadCircleToggle", &config.bEspHeadCircle);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // HeadCircleRow

        // ---- Distance Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("DistanceRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Distance");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##DistanceColorPicker",
                ImVec4(config.espDistanceColor[0], config.espDistanceColor[1], config.espDistanceColor[2], config.espDistanceColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##DistanceColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##DistanceColorPopup")) {
                ImGui::ColorPicker4("##DistanceColorPicker", config.espDistanceColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspDistance);
            ImGui::Checkbox("##DistanceToggle", &config.bEspDistance);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // DistanceRow

        // ---- Snapline Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("SnaplineRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Snapline");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##SnaplineColorPicker",
                ImVec4(config.espSnaplineColor[0], config.espSnaplineColor[1], config.espSnaplineColor[2], config.espSnaplineColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##SnaplineColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##SnaplineColorPopup")) {
                ImGui::ColorPicker4("##SnaplineColorPicker", config.espSnaplineColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspSnapline);
            ImGui::Checkbox("##SnaplineToggle", &config.bEspSnapline);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // SnaplineRow

        // ---- View Direction Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("ViewDirectionRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("View Direction");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##ViewDirectionColorPicker",
                ImVec4(config.espViewDirectionColor[0], config.espViewDirectionColor[1],
                    config.espViewDirectionColor[2], config.espViewDirectionColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##ViewDirectionColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##ViewDirectionColorPopup")) {
                ImGui::ColorPicker4("##ViewDirectionColorPicker", config.espViewDirectionColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                    ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspViewDirection);
            ImGui::Checkbox("##ViewDirectionToggle", &config.bEspViewDirection);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // ViewDirectionRow

        // ---- Weapon Row ----
        
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("WeaponRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Weapon");
            ImGui::SetWindowFontScale(1.0f);

            float colourButtonWidth = 40.0f;
            float colourButtonHeight = 20.0f;
            float gap = 4.0f;
            float colorPickerX = checkboxX - gap - colourButtonWidth;

            ImGui::SameLine();
            ImGui::SetCursorPosX(colorPickerX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
            if (ImGui::ColorButton("##WeaponColorPicker",
                ImVec4(config.espWeaponColor[0], config.espWeaponColor[1], config.espWeaponColor[2], config.espWeaponColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(colourButtonWidth, colourButtonHeight))) {
                ImGui::OpenPopup("##WeaponColorPopup");
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup("##WeaponColorPopup")) {
                ImGui::ColorPicker4("##WeaponColorPicker", config.espWeaponColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bEspWeapon);
            ImGui::Checkbox("##WeaponToggle", &config.bEspWeapon);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // WeaponRow
        
        // ---- Radar Row ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushID("RadarRow");
        {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Radar");
            ImGui::SetWindowFontScale(1.0f);

            float rightEdge = ImGui::GetContentRegionMax().x;
            float checkboxSize = 20.0f;
            float checkboxX = rightEdge - checkboxSize - 5.0f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(checkboxX);
            StyleCheckbox(config.bRadar);
            ImGui::Checkbox("##RadarToggle", &config.bRadar);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID(); // RadarRow

        ImGui::EndChild(); // ESPcard

        // =========================================================
        // GLOW CARD (right)
        // =========================================================
        ImGui::SameLine();
        ImGui::BeginChild("##GlowCard", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);

        // ---- Enable Glow ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Enable Glow");
        ImGui::SetWindowFontScale(1.0f);

        float glowCheckboxX = ImGui::GetContentRegionMax().x - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(glowCheckboxX);
        StyleCheckbox(config.bGlow);
        ImGui::Checkbox("##GlowEnable", &config.bGlow);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Team Check ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Team Check");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(glowCheckboxX);
        StyleCheckbox(config.bGlowTeamCheck);
        ImGui::Checkbox("##GlowTeamCheck", &config.bGlowTeamCheck);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Glow Type Dropdown ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Type");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(100.0f);
        ImGui::PushItemWidth(120.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);

        ImVec4 themeColorGlow = ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f);
        ImVec4 cardBgGlow = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Border, themeColorGlow);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, cardBgGlow);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBgGlow);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, cardBgGlow);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));

        const char* glowTypes[] = { "Regular", "Team Based", "Health Based" };
        if (ImGui::Combo("##GlowType", &config.glowType, glowTypes, IM_ARRAYSIZE(glowTypes))) {}

        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Color pickers based on type ----
        if (config.glowType == 0) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Color");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine(100.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));

            if (ImGui::ColorButton("##GlowColorPicker",
                ImVec4(config.glowColor[0], config.glowColor[1], config.glowColor[2], config.glowColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(80, 20))) {
                ImGui::OpenPopup("##GlowColorPopup");
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            if (ImGui::BeginPopup("##GlowColorPopup")) {
                ImGui::ColorPicker4("##GlowColorPicker", config.glowColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                    ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }
        }
        else if (config.glowType == 1) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Team Color");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine(100.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));

            if (ImGui::ColorButton("##GlowTeamColorPicker",
                ImVec4(config.glowTeamColor[0], config.glowTeamColor[1], config.glowTeamColor[2], config.glowTeamColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(80, 20))) {
                ImGui::OpenPopup("##GlowTeamColorPopup");
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            if (ImGui::BeginPopup("##GlowTeamColorPopup")) {
                ImGui::ColorPicker4("##GlowTeamColorPicker", config.glowTeamColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                    ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }

            ImGui::Dummy(ImVec2(0, 4.0f));

            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Enemy Color");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine(100.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));

            if (ImGui::ColorButton("##GlowEnemyColorPicker",
                ImVec4(config.glowEnemyColor[0], config.glowEnemyColor[1], config.glowEnemyColor[2], config.glowEnemyColor[3]),
                ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
                ImVec2(80, 20))) {
                ImGui::OpenPopup("##GlowEnemyColorPopup");
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            if (ImGui::BeginPopup("##GlowEnemyColorPopup")) {
                ImGui::ColorPicker4("##GlowEnemyColorPicker", config.glowEnemyColor,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                    ImGuiColorEditFlags_AlphaBar);
                ImGui::EndPopup();
            }
        }

        ImGui::EndChild(); // GlowCard

        ImGui::PopStyleVar(); // WindowPadding
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    // ============================================================
    // MISC
    // ============================================================
    else if (currentCategory == 2) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float cardWidth = (avail.x - 10.0f) * 0.5f;
        float cardHeight = avail.y;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));

        // ---- MISC CARD ----
        ImGui::BeginChild("##MiscCard", ImVec2(cardWidth, cardHeight), ImGuiChildFlags_Borders);

        // ---- Bunny Hop ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Bunny Hop");
        ImGui::SetWindowFontScale(1.0f);

        float rightEdge = ImGui::GetContentRegionMax().x;
        float checkboxSize = 20.0f;
        float checkboxX = rightEdge - checkboxSize - 5.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bBhop);
        ImGui::Checkbox("##BhopToggle", &config.bBhop);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Remove Flash ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Remove Flash");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bNoFlash);
        ImGui::Checkbox("##NoFlashToggle", &config.bNoFlash);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- FOV Changer ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("FOV Changer");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bFovChanger);
        ImGui::Checkbox("##FovChangerToggle", &config.bFovChanger);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- FOV Slider (only if enabled) ----
        if (config.bFovChanger) {
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("FOV Value");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine(70.0f);
            float sliderWidth = (ImGui::GetContentRegionMax().x - 70.0f - 5.0f);
            ImGui::PushItemWidth(sliderWidth);
            SliderFloatStyled("##FovValue", &config.fovValue, 60.0f, 120.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4.0f));
        }

        // ---- Bomb Timer ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Bomb Timer");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bBombTimer);
        ImGui::Checkbox("##BombTimerToggle", &config.bBombTimer);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Streamproof ----
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("Streamproof");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(checkboxX);
        StyleCheckbox(config.bStreamproof);
        ImGui::Checkbox("##StreamproofToggle", &config.bStreamproof);
        ImGui::PopStyleColor(4);
        ImGui::Dummy(ImVec2(0, 4.0f));

        // ---- Menu Key ----
        ImGui::Dummy(ImVec2(0, 4.0f));
        KeyBind("menukey", "Menu Key", &config.menuKey);

        // ---- Unload Button ----
        ImGui::Dummy(ImVec2(0, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
        float availWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Unload", ImVec2(availWidth, 40))) {
            g_unload_requested = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::Dummy(ImVec2(0, 4.0f));

        ImGui::EndChild(); // MiscCard

        ImGui::PopStyleVar(); // WindowPadding
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    // ============================================================
    // CONFIG
    // ============================================================
    else if (currentCategory == 3) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##ConfigCard", avail, ImGuiChildFlags_Borders);

        static std::vector<std::string> allConfigs, filteredConfigs;
        static std::string searchText;
        static int selectedIndex = -1;
        static bool showCreatePopup = false;
        static char createNameBuffer[256] = "";
        static bool refreshList = true;
        static char renameBuffer[256] = "";

        if (refreshList) {
            allConfigs = Config::GetConfigList();
            refreshList = false;
            if (selectedIndex >= (int)allConfigs.size()) selectedIndex = -1;
        }
        filteredConfigs.clear();
        std::string searchLower = searchText;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        for (const auto& f : allConfigs) {
            std::string fLower = f;
            std::transform(fLower.begin(), fLower.end(), fLower.begin(), ::tolower);
            if (fLower.find(searchLower) != std::string::npos) filteredConfigs.push_back(f);
        }

        // ---- Search bar ----
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Search");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
        char searchBuf[256];
        strncpy_s(searchBuf, sizeof(searchBuf), searchText.c_str(), _TRUNCATE);
        if (ImGui::InputText("##Search", searchBuf, sizeof(searchBuf))) searchText = searchBuf;
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();

        // ---- Config list ----
        float remainingHeight = ImGui::GetContentRegionAvail().y - 2.0f;
        float buttonRowsHeight = 30.0f + 0.0f + 30.0f + 2.0f + 10.0f;
        float listHeight = remainingHeight - buttonRowsHeight;
        if (listHeight < 30.0f) listHeight = 30.0f;

        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.9686f, 0.7961f, 0.8196f, 0.8f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 2.0f);

        ImGui::BeginChild("##ConfigList", ImVec2(0, listHeight), true);
        for (int i = 0; i < (int)filteredConfigs.size(); i++) {
            bool isSelected = (selectedIndex == i);
            if (ImGui::Selectable(filteredConfigs[i].c_str(), isSelected)) {
                selectedIndex = i;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // ---- Row 1: Create, Load, Save ----
        float buttonWidth = (ImGui::GetContentRegionAvail().x - 20.0f) / 3.0f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
        if (ImGui::Button("Create Config", ImVec2(buttonWidth, 30))) {
            showCreatePopup = true;
            memset(createNameBuffer, 0, sizeof(createNameBuffer));
        }
        ImGui::SameLine();
        bool loadDisabled = (selectedIndex < 0 || selectedIndex >= (int)filteredConfigs.size());
        if (loadDisabled) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        }
        if (ImGui::Button("Load Config", ImVec2(buttonWidth, 30)) && !loadDisabled) {
            Config::LoadConfig(filteredConfigs[selectedIndex]);
        }
        if (loadDisabled) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        bool saveDisabled = (selectedIndex < 0 || selectedIndex >= (int)filteredConfigs.size());
        if (saveDisabled) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        }
        if (ImGui::Button("Save Config", ImVec2(buttonWidth, 30)) && !saveDisabled) {
            Config::SaveConfig(filteredConfigs[selectedIndex]);
        }
        if (saveDisabled) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor(3);

        // ---- Row 2: Delete, Rename, Open Path ----
        buttonWidth = (ImGui::GetContentRegionAvail().x - 20.0f) / 3.0f;

        // Delete
        bool delDisabled = (selectedIndex < 0 || selectedIndex >= (int)filteredConfigs.size());
        if (delDisabled) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.0f, 0.0f, 1.0f));
        }
        if (ImGui::Button("Delete", ImVec2(buttonWidth, 30)) && !delDisabled) {
            ImGui::OpenPopup("Delete Config##Confirm");
        }
        if (delDisabled) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        else {
            ImGui::PopStyleColor(3);
        }

        // Rename
        ImGui::SameLine();
        bool renameDisabled = (selectedIndex < 0 || selectedIndex >= (int)filteredConfigs.size());
        if (renameDisabled) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
        }
        if (ImGui::Button("Rename", ImVec2(buttonWidth, 30)) && !renameDisabled) {
            std::string name = filteredConfigs[selectedIndex];
            if (name.size() > 4 && name.substr(name.size() - 4) == ".ini") {
                name = name.substr(0, name.size() - 4);
            }
            strcpy_s(renameBuffer, name.c_str());
            ImGui::OpenPopup("Rename Config");
        }
        if (renameDisabled) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        else {
            ImGui::PopStyleColor(3);
        }

        // Open Path
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
        if (ImGui::Button("Open Path", ImVec2(buttonWidth, 30))) {
            ShellExecuteA(nullptr, "open", "C:\\Velocity\\configs", nullptr, nullptr, SW_SHOW);
        }
        ImGui::PopStyleColor(3);

        // ---- Popups ----
        // Delete Confirmation
        if (ImGui::BeginPopupModal("Delete Config##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete '%s'?", filteredConfigs[selectedIndex].c_str());
            ImGui::Spacing();
            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                if (Config::DeleteConfig(filteredConfigs[selectedIndex])) {
                    refreshList = true;
                    selectedIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Rename Popup
        if (ImGui::BeginPopupModal("Rename Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter new name (without .ini):");
            ImGui::InputText("##RenameInput", renameBuffer, sizeof(renameBuffer));
            ImGui::Spacing();
            if (ImGui::Button("Rename", ImVec2(120, 0))) {
                std::string newName = renameBuffer;
                if (!newName.empty()) {
                    if (newName.find(".ini") == std::string::npos)
                        newName += ".ini";
                    std::string oldName = filteredConfigs[selectedIndex];
                    if (Config::RenameConfig(oldName, newName)) {
                        refreshList = true;
                        selectedIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Create Config Popup
        if (showCreatePopup) ImGui::OpenPopup("Create Config");
        if (ImGui::BeginPopupModal("Create Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter new config name:");
            ImGui::InputText("##ConfigName", createNameBuffer, sizeof(createNameBuffer));
            ImGui::Spacing();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                std::string newName = createNameBuffer;
                if (!newName.empty()) {
                    if (Config::CreateConfig(newName)) {
                        refreshList = true;
                        showCreatePopup = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showCreatePopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}