#include "Radar.h"
#include "Config.h"
#include "Memory.h"
#include "Offsets.h"
#include "Overlay.h"
#include "SDK.h"               // for Vector3, M_PI_F
#include "imgui.h"
#include <d3d11.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <optional>
#include <filesystem>
#include <iostream>
#include <psapi.h>          // for module information

#include "stb_image.h"

extern Memory mem;
extern ID3D11Device* g_pd3dDevice;

namespace Radar
{
    struct MapCoords
    {
        float posX, posY;
        float scale;
    };

    struct MapData
    {
        ID3D11ShaderResourceView* texture = nullptr;
        int width = 0, height = 0;
        MapCoords coords{ 0.f, 0.f, 1.f };
        std::string lastError;
    };

    struct RadarPlayer
    {
        Vector3 pos;
        Vector3 eyeAngles;      // view direction (yaw)
        int team;
        bool self;
    };

    static std::unordered_map<std::string, MapData> s_mapTextures;
    static std::string s_currentMapName;
    static std::vector<RadarPlayer> s_players;
    static Vector3 s_localPos;
    static bool s_hasLocal = false;
    static int s_playerCount = 0;

    // ---- Default radar cvar control ----
    static float* g_pRadarScaleCvar = nullptr;
    static float  g_OriginalRadarScale = 1.0f;
    static bool   g_hasCvar = false;
    static float  g_currentScale = 1.0f;
    static uintptr_t g_cvarAddress = 0;

    // Fallback coordinates (used only if .txt is missing)
    static const std::unordered_map<std::string, MapCoords> s_coordsMap = {
        { "cs_italy",   { -2647.f, 2592.f, 4.6f } },
        { "cs_office",  { -1838.f, 1858.f, 4.1f } },
        { "de_ancient", { -2953.f, 2164.f, 5.f } },
        { "de_anubis",  { -2796.f, 3328.f, 5.22f } },
        { "de_dust",    { -2850.f, 4073.f, 6.f } },
        { "de_dust2",   { -2476.f, 3239.f, 4.4f } },
        { "de_inferno", { -2087.f, 3870.f, 4.9f } },
        { "de_mirage",  { -3230.f, 1713.f, 5.f } },
        { "de_nuke",    { -3453.f, 2887.f, 7.f } },
        { "de_overpass",{ -4831.f, 1781.f, 5.2f } },
        { "de_vertigo", { -3168.f, 1762.f, 4.f } },
    };

    static MapCoords ParseCoordsFile(const std::string& txtPath)
    {
        MapCoords coords{ 0.f, 0.f, 1.f };
        std::ifstream file(txtPath);
        if (!file.is_open())
            return coords;

        std::string line;
        while (std::getline(file, line))
        {
            auto parseVal = [](const std::string& l, const std::string& key) -> std::optional<float> {
                size_t k = l.find(key);
                if (k == std::string::npos)
                    return std::nullopt;
                size_t q1 = l.find('"', k + key.size());
                if (q1 == std::string::npos)
                    return std::nullopt;
                size_t q2 = l.find('"', q1 + 1);
                if (q2 == std::string::npos)
                    return std::nullopt;
                return std::stof(l.substr(q1 + 1, q2 - q1 - 1));
                };

            if (auto v = parseVal(line, "\"pos_x\""))
                coords.posX = *v;
            if (auto v = parseVal(line, "\"pos_y\""))
                coords.posY = *v;
            if (auto v = parseVal(line, "\"scale\""))
                coords.scale = *v;
        }
        return coords;
    }

    // Corrected mapping: no abs(), flip Y because world Y increases north but screen Y increases downward
    static ImVec2 WorldToRadar(const Vector3& worldPos, const MapCoords& coords)
    {
        float x = (worldPos.x - coords.posX) / coords.scale - 10.f;
        float y = (coords.posY - worldPos.y) / coords.scale - 10.f;
        return { x, y };
    }

    static ID3D11ShaderResourceView* LoadTextureFromFile(const std::string& path, int& outWidth, int& outHeight, std::string& outError)
    {
        outError.clear();

        if (!std::filesystem::exists(path))
        {
            outError = "File not found: " + path;
            return nullptr;
        }

        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            const char* reason = stbi_failure_reason();
            outError = "stbi_load failed: " + std::string(reason ? reason : "unknown");
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
        if (FAILED(hr))
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "CreateTexture2D failed: 0x%08X", hr);
            outError = buf;
            stbi_image_free(data);
            return nullptr;
        }
        hr = g_pd3dDevice->CreateShaderResourceView(pTexture, nullptr, &textureView);
        if (FAILED(hr))
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "CreateSRV failed: 0x%08X", hr);
            outError = buf;
            pTexture->Release();
            stbi_image_free(data);
            return nullptr;
        }
        pTexture->Release();
        stbi_image_free(data);

        outWidth = width;
        outHeight = height;
        return textureView;
    }

    static MapCoords GetCoordsForMap(const std::string& mapName)
    {
        auto it = s_coordsMap.find(mapName);
        if (it != s_coordsMap.end())
            return it->second;
        return { 0.f, 0.f, 1.f };
    }

    static bool LoadMapTexture(const std::string& mapName, MapData& outData)
    {
        outData.coords = GetCoordsForMap(mapName);
        outData.texture = nullptr;
        outData.width = 0;
        outData.height = 0;
        outData.lastError.clear();

        // Load coordinates from .txt (preferred)
        std::string txtPath1 = "C:\\Velocity\\assets\\radar\\ar_" + mapName + ".txt";
        std::string txtPath2 = "C:\\Velocity\\assets\\radar\\" + mapName + ".txt";
        MapCoords txtCoords{ 0.f, 0.f, 0.f };
        bool gotCoords = false;
        if (std::filesystem::exists(txtPath1))
        {
            txtCoords = ParseCoordsFile(txtPath1);
            if (txtCoords.scale > 0.f) gotCoords = true;
        }
        if (!gotCoords && std::filesystem::exists(txtPath2))
        {
            txtCoords = ParseCoordsFile(txtPath2);
            if (txtCoords.scale > 0.f) gotCoords = true;
        }
        if (gotCoords)
            outData.coords = txtCoords;

        // Try two filenames: with "ar_" and without
        std::vector<std::string> pathsToTry = {
            "C:\\Velocity\\assets\\radar\\ar_" + mapName + ".png",
            "C:\\Velocity\\assets\\radar\\" + mapName + ".png"
        };

        for (const auto& pngPath : pathsToTry)
        {
            int w, h;
            std::string error;
            ID3D11ShaderResourceView* tex = LoadTextureFromFile(pngPath, w, h, error);
            if (tex)
            {
                outData.texture = tex;
                outData.width = w;
                outData.height = h;
                return true;
            }
            else
            {
                outData.lastError = error;
            }
        }

        // ---- FALLBACK: load empty.png if map texture is missing ----
        std::string fallbackPath = "C:\\Velocity\\assets\\radar\\empty.png";
        int w, h;
        std::string error;
        ID3D11ShaderResourceView* tex = LoadTextureFromFile(fallbackPath, w, h, error);
        if (tex)
        {
            outData.texture = tex;
            outData.width = w;
            outData.height = h;
            outData.lastError = "Using fallback empty.png";
            return true;
        }
        else
        {
            outData.lastError = "No texture found (map nor fallback) - " + error;
            return false;
        }
    }

    // ---- Enhanced scanner for cl_radar_scale ----
    static float* FindRadarScaleCvar() {
        const wchar_t* modules[] = { L"client.dll", L"engine2.dll" };
        for (auto modName : modules) {
            HMODULE hMod = GetModuleHandleW(modName);
            if (!hMod) continue;

            MODULEINFO modInfo;
            if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo)))
                continue;

            uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
            uintptr_t size = modInfo.SizeOfImage;

            const char* pattern = "cl_radar_scale";
            size_t patternLen = strlen(pattern);

            // Scan for the string
            for (uintptr_t i = base; i < base + size - patternLen; ++i) {
                if (memcmp((void*)i, pattern, patternLen) == 0) {
                    // Found the string. Now scan for a pointer to a float near it.
                    // The ConVar object typically has the value pointer at offsets 0x10, 0x18, 0x20, etc.
                    // We'll scan a wide range.
                    for (int off = -0x40; off <= 0x40; off += 0x4) {
                        uintptr_t ptrAddr = i + off;
                        if (ptrAddr < base || ptrAddr >= base + size - 4) continue;

                        // Read a potential pointer
                        uintptr_t valPtr = *(uintptr_t*)ptrAddr;
                        if (valPtr < base || valPtr >= base + size) continue;

                        // Check if it points to a valid float (0-180)
                        __try {
                            float val = *(float*)valPtr;
                            if (val >= 0.0f && val <= 180.0f) {
                                // Heuristic: the closer to the string, the more likely.
                                // We'll return the first valid one.
                                g_cvarAddress = ptrAddr;
                                return (float*)valPtr;
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER) {
                            continue;
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    // ---- Public API ----
    void SetDefaultRadarVisible(bool visible) {
        if (!g_hasCvar) {
            g_pRadarScaleCvar = FindRadarScaleCvar();
            if (g_pRadarScaleCvar) {
                g_OriginalRadarScale = *g_pRadarScaleCvar;
                g_hasCvar = true;
            }
            else {
                // If not found, we'll retry later (maybe module not loaded yet)
                return;
            }
        }
        if (g_pRadarScaleCvar) {
            *g_pRadarScaleCvar = visible ? g_OriginalRadarScale : 0.0f;
            g_currentScale = *g_pRadarScaleCvar;
        }
    }

    void Update()
    {
        // ---- Toggle default radar visibility ----
        static bool lastRadarState = false;
        bool currentRadarState = config.bRadar;

        if (currentRadarState != lastRadarState) {
            SetDefaultRadarVisible(!currentRadarState);
            lastRadarState = currentRadarState;
        }

        // If our radar is off, stop reading data but keep the scale as set
        if (!currentRadarState) {
            s_players.clear();
            return;
        }

        // ---- Force the hidden scale every frame (0.0f) ----
        if (g_hasCvar && g_pRadarScaleCvar) {
            *g_pRadarScaleCvar = 0.0f;
            g_currentScale = 0.0f;
        }
        else {
            // Try to find it again (in case it wasn't found earlier)
            g_pRadarScaleCvar = FindRadarScaleCvar();
            if (g_pRadarScaleCvar) {
                g_OriginalRadarScale = *g_pRadarScaleCvar;
                g_hasCvar = true;
                *g_pRadarScaleCvar = 0.0f;
                g_currentScale = 0.0f;
            }
        }

        // ---- Read map name ----
        std::string currentMap;
        uintptr_t globalVars = mem.Read<uintptr_t>(mem.client + offsets::client::dwGlobalVars);
        if (globalVars)
        {
            for (uintptr_t off : { 0x188, 0x190, 0x1A0 })
            {
                uintptr_t ptr = mem.Read<uintptr_t>(globalVars + off);
                if (ptr)
                {
                    char buf[64] = {};
                    mem.ReadRaw(ptr, buf, sizeof(buf) - 1);
                    std::string s(buf);
                    if (!s.empty())
                    {
                        currentMap = s;
                        break;
                    }
                }
            }
            if (currentMap.empty())
            {
                char buf[128] = {};
                mem.ReadRaw(globalVars + 0x190, buf, sizeof(buf) - 1);
                currentMap = buf;
                if (currentMap.find_first_not_of("abcdefghijklmnopqrstuvwxyz_") == std::string::npos)
                    currentMap.clear();
            }
        }

        if (currentMap.empty())
        {
            uintptr_t gameRules = mem.Read<uintptr_t>(mem.client + offsets::client::dwGameRules);
            if (gameRules)
            {
                for (uintptr_t off : { 0x190, 0x1B0, 0x1C0 })
                {
                    uintptr_t ptr = mem.Read<uintptr_t>(gameRules + off);
                    if (ptr)
                    {
                        char buf[64] = {};
                        mem.ReadRaw(ptr, buf, sizeof(buf) - 1);
                        std::string s(buf);
                        if (!s.empty())
                        {
                            currentMap = s;
                            break;
                        }
                    }
                }
            }
        }

        if (!currentMap.empty())
        {
            size_t slash = currentMap.rfind('/');
            if (slash != std::string::npos)
                currentMap = currentMap.substr(slash + 1);
            size_t dot = currentMap.rfind('.');
            if (dot != std::string::npos)
                currentMap = currentMap.substr(0, dot);
        }

        if (!currentMap.empty() && currentMap != s_currentMapName)
        {
            s_currentMapName = currentMap;
            MapData md;
            LoadMapTexture(s_currentMapName, md);
            s_mapTextures[s_currentMapName] = md;
        }

        // ---- Read players ----
        s_players.clear();
        s_hasLocal = false;
        s_playerCount = 0;

        uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
        uintptr_t localController = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerController);
        if (!localPawn || !localController) return;

        uintptr_t localSceneNode = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
        if (localSceneNode)
            s_localPos = mem.Read<Vector3>(localSceneNode + offsets::sceneNode::m_vecAbsOrigin);
        else
            s_localPos = { 0,0,0 };

        uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);

        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) return;

        for (int i = 1; i < 64; i++)
        {
            uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9) + 16));
            if (!listEntry) continue;

            uintptr_t controller = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!controller || controller == localController) continue;

            uint32_t pawnHandle = mem.Read<uint32_t>(controller + offsets::controller::m_hPlayerPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;

            uintptr_t pawnEntry = mem.Read<uintptr_t>(entityList + (8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
            if (!pawnEntry) continue;

            uintptr_t pawn = mem.Read<uintptr_t>(pawnEntry + 112 * ((pawnHandle & 0x7FFF) & 0x1FF));
            if (!pawn || pawn == localPawn) continue;

            int health = mem.Read<int>(pawn + offsets::entity::m_iHealth);
            if (health <= 0 || health > 100) continue;

            uint8_t team = mem.Read<uint8_t>(pawn + offsets::entity::m_iTeamNum);

            uintptr_t sceneNode = mem.Read<uintptr_t>(pawn + offsets::entity::m_pGameSceneNode);
            if (!sceneNode) continue;
            if (mem.Read<bool>(sceneNode + offsets::sceneNode::m_bDormant)) continue;

            Vector3 pos = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
            Vector3 eyeAngles = mem.Read<Vector3>(pawn + offsets::csPawn::m_angEyeAngles);

            RadarPlayer p;
            p.pos = pos;
            p.eyeAngles = eyeAngles;
            p.team = team;
            p.self = false;
            s_players.push_back(p);
            s_playerCount++;
        }

        // Add local player
        RadarPlayer self;
        self.pos = s_localPos;
        self.eyeAngles = mem.Read<Vector3>(localPawn + offsets::csPawn::m_angEyeAngles);
        self.team = localTeam;
        self.self = true;
        s_players.push_back(self);
        s_playerCount++;
        s_hasLocal = true;
    }

    void Draw()
    {
        if (!config.bRadar) return;

        const float radarSize = 300.f;
        const float padding = 20.f;

        ImGui::SetNextWindowSize(ImVec2(radarSize + padding * 2, radarSize + padding * 2), ImGuiCond_Once);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("Radar", nullptr, flags);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Background
        draw->AddRectFilled(pos, ImVec2(pos.x + radarSize, pos.y + radarSize), IM_COL32(0, 0, 0, 180));

        // Get map data
        MapData mapData;
        bool hasMapData = false;
        if (!s_currentMapName.empty())
        {
            auto it = s_mapTextures.find(s_currentMapName);
            if (it != s_mapTextures.end())
            {
                mapData = it->second;
                hasMapData = true;
            }
            else
            {
                mapData.coords = GetCoordsForMap(s_currentMapName);
                hasMapData = true;
            }
        }

        if (!hasMapData)
            mapData.coords = { 0.f, 0.f, 1.f };

        // Draw texture if available
        if (hasMapData && mapData.texture)
        {
            ImGui::Image(mapData.texture, ImVec2(radarSize, radarSize));
        }
        else
        {
            // If no texture, draw a simple placeholder rectangle and an error message (minimal)
            draw->AddRect(pos, ImVec2(pos.x + radarSize, pos.y + radarSize), IM_COL32(255, 255, 255, 40));
            const char* msg = mapData.lastError.empty() ? "No texture" : mapData.lastError.c_str();
            ImVec2 ts = ImGui::CalcTextSize(msg);
            draw->AddText(ImVec2(pos.x + (radarSize - ts.x) * 0.5f, pos.y + (radarSize - ts.y) * 0.5f),
                IM_COL32(255, 100, 100, 200), msg);
        }

        // Get texture dimensions for scaling
        float texWidth = (mapData.width > 0) ? static_cast<float>(mapData.width) : 1024.f;
        float texHeight = (mapData.height > 0) ? static_cast<float>(mapData.height) : 1024.f;

        // Draw players
        for (const auto& player : s_players)
        {
            ImVec2 rp = WorldToRadar(player.pos, mapData.coords);
            // Clamp to texture size
            if (rp.x < 0.f) rp.x = 0.f;
            if (rp.x > texWidth) rp.x = texWidth;
            if (rp.y < 0.f) rp.y = 0.f;
            if (rp.y > texHeight) rp.y = texHeight;

            float scaleX = radarSize / texWidth;
            float scaleY = radarSize / texHeight;
            float x = pos.x + rp.x * scaleX;
            float y = pos.y + rp.y * scaleY;

            ImU32 col;
            if (player.self)
                col = IM_COL32(0, 255, 0, 255);
            else if (player.team == 2)
                col = IM_COL32(255, 180, 0, 255);
            else if (player.team == 3)
                col = IM_COL32(100, 180, 255, 255);
            else
                col = IM_COL32(200, 200, 200, 255);

            draw->AddCircleFilled(ImVec2(x, y), 5.f, col);
            draw->AddCircle(ImVec2(x, y), 5.f, IM_COL32(0, 0, 0, 180));

            // ---- View direction arrow with semi-transparent fill ----
            // Compute direction vector from yaw (radians)
            float yawRad = player.eyeAngles.y * (M_PI_F / 180.0f);
            Vector3 dir(cosf(yawRad), sinf(yawRad), 0.0f);

            // FIX: invert direction because the arrow was opposite
            dir = dir * -1.0f;

            // End point 50 units ahead in world space
            Vector3 endWorld = player.pos + dir * 50.0f;

            // Transform both to radar pixel coords using the corrected mapping
            ImVec2 rpStart = WorldToRadar(player.pos, mapData.coords);
            ImVec2 rpEnd = WorldToRadar(endWorld, mapData.coords);

            // Clamp to texture bounds
            rpStart.x = std::clamp(rpStart.x, 0.0f, texWidth);
            rpStart.y = std::clamp(rpStart.y, 0.0f, texHeight);
            rpEnd.x = std::clamp(rpEnd.x, 0.0f, texWidth);
            rpEnd.y = std::clamp(rpEnd.y, 0.0f, texHeight);

            float sx = radarSize / texWidth;
            float sy = radarSize / texHeight;
            ImVec2 screenStart(pos.x + rpStart.x * sx, pos.y + rpStart.y * sy);
            ImVec2 screenEnd(pos.x + rpEnd.x * sx, pos.y + rpEnd.y * sy);

            // Compute arrowhead points
            float dx = screenEnd.x - screenStart.x;
            float dy = screenEnd.y - screenStart.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1.0f) {
                float ux = dx / len;
                float uy = dy / len;
                float px = -uy;                     // perpendicular
                float py = ux;
                float headLen = 10.0f;
                float headWidth = 6.0f;
                float tipX = screenEnd.x;
                float tipY = screenEnd.y;
                float baseX = tipX - ux * headLen;
                float baseY = tipY - uy * headLen;
                float leftX = baseX + px * headWidth;
                float leftY = baseY + py * headWidth;
                float rightX = baseX - px * headWidth;
                float rightY = baseY - py * headWidth;

                // Use the same alpha for both fill and lines (150 = semi‑transparent)
                constexpr int arrowAlpha = 150;
                ImU32 colAlpha = (col & 0x00FFFFFF) | (arrowAlpha << 24);

                // Fill the arrowhead triangle
                draw->AddTriangleFilled(
                    ImVec2(tipX, tipY),
                    ImVec2(leftX, leftY),
                    ImVec2(rightX, rightY),
                    colAlpha
                );

                // Draw all three lines with the identical alpha
                draw->AddLine(screenStart, ImVec2(tipX, tipY), colAlpha, 2.0f);
                draw->AddLine(ImVec2(tipX, tipY), ImVec2(leftX, leftY), colAlpha, 2.0f);
                draw->AddLine(ImVec2(tipX, tipY), ImVec2(rightX, rightY), colAlpha, 2.0f);
            }
        }

        // Border
        draw->AddRect(pos, ImVec2(pos.x + radarSize, pos.y + radarSize), IM_COL32(255, 255, 255, 80));

        ImGui::End();
    }

    void Cleanup()
    {
        // Restore default radar scale
        if (g_pRadarScaleCvar) {
            *g_pRadarScaleCvar = g_OriginalRadarScale;
            g_pRadarScaleCvar = nullptr;
        }
        g_hasCvar = false;

        for (auto& pair : s_mapTextures)
        {
            if (pair.second.texture)
            {
                pair.second.texture->Release();
                pair.second.texture = nullptr;
            }
        }
        s_mapTextures.clear();
        s_currentMapName.clear();
        s_players.clear();
    }
}