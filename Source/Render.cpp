#include "Render.h"
#include "Config.h"
#include "Memory.h"
#include "Offsets.h"
#include "SDK.h"
#include "Box.h"
#include "Skeleton.h"
#include "Name.h"
#include "HealthBar.h"
#include "ArmorBar.h"
#include "HeadCircle.h"
#include "Distance.h"
#include "Snapline.h"
#include "ViewDirection.h"
#include "Weapon.h"
#include "Overlay.h"        // för g_entityCache
#include "imgui.h"

extern Memory mem;
extern Config config;

void RenderESP(int screenWidth, int screenHeight) {
    if (!config.bEsp) return;

    // Läs viewmatrix och lokal spelare
    Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(mem.client + offsets::client::dwViewMatrix);
    uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
    if (!localPawn) return;
    uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);

    // Hämta cachade entities
    const auto& entities = g_entityCache.Get();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (const auto& ent : entities) {
        if (!ent.valid) continue;
        if (config.bEspTeamCheck && ent.team == localTeam) continue;

        // World‑to‑screen
        Vector3 headTop = ent.headPos + Vector3{ 0, 0, 8.0f };
        Vector2 screenHead, screenFeet;
        if (!WorldToScreen(headTop, screenHead, viewMatrix, screenWidth, screenHeight)) continue;
        if (!WorldToScreen(ent.origin, screenFeet, viewMatrix, screenWidth, screenHeight)) continue;

        float boxHeight = screenFeet.y - screenHead.y;
        if (boxHeight < 4.0f) continue;

        // Bygg temporär EntityData från cachen (för att passa befintliga ritfunktioner)
        EntityData temp;
        temp.valid = true;
        temp.pawn = ent.pawn;
        temp.controller = ent.controller;
        temp.health = ent.health;
        temp.armor = ent.armor;
        temp.team = ent.team;
        temp.origin = ent.origin;
        temp.headPos = ent.headPos;
        temp.distance = ent.distance;
        temp.bonesValid = ent.bonesValid;
        memcpy(temp.bones, ent.bones, sizeof(ent.bones));
        strcpy_s(temp.name, ent.name);
        strcpy_s(temp.weaponName, ent.weaponName);

        // ---- Rita ESP-komponenter ----
        if (config.bEspBox) {
            ESP::DrawBox(screenHead, screenFeet,
                config.espBoxColor, config.espFillColor,
                config.boxType, config.boxThickness, config.bBoxFill);
        }

        if (config.bEspSkeleton && temp.bonesValid) {
            ESP::DrawSkeleton(temp.bones, viewMatrix, screenWidth, screenHeight, config.espSkeletonColor);
        }

        if (config.bEspName && temp.name[0]) {
            ESP::DrawName(screenHead, temp.name, config.espNameColor);
        }

        if (config.bEspHealthBar) {
            ESP::DrawHealthBar(screenHead, screenFeet, temp.health, config.espHealthColor);
        }

        if (config.bEspArmorBar && temp.armor > 0) {
            ESP::DrawArmorBar(screenHead, screenFeet, temp.armor, config.espArmorColor);
        }

        if (config.bEspHeadCircle && temp.bonesValid) {
            ESP::DrawHeadCircle(temp, viewMatrix, screenWidth, screenHeight);
        }

        if (config.bEspWeapon && temp.weaponName[0]) {
            ImVec2 textSize = ImGui::CalcTextSize(temp.weaponName);
            float x = screenFeet.x - textSize.x * 0.5f;
            float y = screenFeet.y + 1.0f;
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                config.espWeaponColor[0],
                config.espWeaponColor[1],
                config.espWeaponColor[2],
                config.espWeaponColor[3]
            ));
            draw->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 255), temp.weaponName);
            draw->AddText(ImVec2(x, y), col, temp.weaponName);
        }

        if (config.bEspDistance) {
            ESP::DrawDistance(screenFeet, temp.distance, config.espDistanceColor);
        }

        if (config.bEspSnapline) {
            ESP::DrawSnapline(screenFeet, screenWidth, screenHeight, config.espSnaplineColor);
        }

        if (config.bEspViewDirection && temp.bonesValid) {
            // Använd den cachade vyvinkeln
            ESP::DrawViewDirection(temp, viewMatrix, screenWidth, screenHeight,
                config.espViewDirectionColor, config.espViewDirectionLength,
                ent.viewAngles);
        }
    }
}