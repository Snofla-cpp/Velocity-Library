#include "HeadShotLine.h"
#include "Config.h"
#include "Memory.h"
#include "Offsets.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

extern Memory mem;

// Use the correct head bone index (6, not 7)
constexpr int HEAD_BONE = 6;

static Vector3 GetHeadPosition(uintptr_t pawn) {
    uintptr_t sceneNode = mem.Read<uintptr_t>(pawn + offsets::entity::m_pGameSceneNode);
    if (!sceneNode) return Vector3{ 0,0,0 };

    // Try to read bone matrix
    uintptr_t boneMatrix = mem.Read<uintptr_t>(sceneNode + offsets::skeleton::m_modelState + 0x80);
    if (boneMatrix) {
        Vector3 headPos = mem.Read<Vector3>(boneMatrix + HEAD_BONE * 32);
        if (!headPos.IsZero()) return headPos;
    }

    // Fallback: origin + approximate head height (72 units)
    Vector3 origin = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
    return origin + Vector3{ 0, 0, 72.0f };
}

void HeadShotLine::Draw(uintptr_t localPawn, const Matrix4x4& viewMatrix,
    int screenWidth, int screenHeight, const float* color) {
    if (!localPawn || !config.bHeadShotLine) return;
    if (!color || color[3] < 0.01f) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    float centerX = screenWidth * 0.5f;
    float halfLen = 50.0f;
    float lineY = screenHeight * 0.5f; // fallback

    // ---- 1) Try to draw on the enemy under crosshair ----
    int crosshairId = mem.Read<int>(localPawn + offsets::csPawn::m_iIDEntIndex);
    if (crosshairId > 0) {
        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (entityList) {
            uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * ((crosshairId & 0x7FFF) >> 9) + 16));
            if (listEntry) {
                uintptr_t entity = mem.Read<uintptr_t>(listEntry + 112 * ((crosshairId & 0x7FFF) & 0x1FF));
                if (entity) {
                    int health = mem.Read<int>(entity + offsets::entity::m_iHealth);
                    uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);
                    uint8_t entityTeam = mem.Read<uint8_t>(entity + offsets::entity::m_iTeamNum);
                    if (health > 0 && health <= 100 && entityTeam != localTeam) {
                        uintptr_t scene = mem.Read<uintptr_t>(entity + offsets::entity::m_pGameSceneNode);
                        if (scene && !mem.Read<bool>(scene + offsets::sceneNode::m_bDormant)) {
                            Vector3 headPos = GetHeadPosition(entity);
                            Vector2 screenPos;
                            if (WorldToScreen(headPos, screenPos, viewMatrix, screenWidth, screenHeight)) {
                                // Adjust slightly upward (optional) – try -4 to move line up
                                lineY = screenPos.y - 14.0f; // remove this if you want exact center
                                goto draw_line;
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- 2) Fallback: static line based on pitch and FOV ----
    {
        uintptr_t cameraServices = mem.Read<uintptr_t>(localPawn + offsets::csPawn::m_pCameraServices);
        if (cameraServices) {
            int fov = mem.Read<int>(cameraServices + offsets::cameraServices::m_iFOV);
            if (fov <= 0) fov = 90;
            Vector3 viewAngles = mem.Read<Vector3>(localPawn + offsets::csPawn::m_angEyeAngles);
            float fovRad = fov * (M_PI_F / 180.0f);
            float pitchRad = viewAngles.x * (M_PI_F / 180.0f);
            lineY = screenHeight / 2.0f -
                (screenHeight / (2.0f * std::sin(fovRad) / std::sin(M_PI_F / 2.0f))) *
                (std::sin(pitchRad) / std::sin(M_PI_F / 2.0f));
            lineY = std::clamp(lineY, 0.0f, static_cast<float>(screenHeight));
        }
    }

draw_line:
    // ---- Draw the horizontal line ----
    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
    draw->AddLine(ImVec2(centerX - halfLen, lineY),
        ImVec2(centerX + halfLen, lineY), col, 2.0f);
}