#include "ViewDirection.h"
#include "imgui.h"
#include <cmath>
#include <vector>

static Vector3 AngleToDirection(const Vector3& angle) {
    float pitch = angle.x * (M_PI_F / 180.0f);
    float yaw = angle.y * (M_PI_F / 180.0f);
    Vector3 dir;
    dir.x = cos(pitch) * cos(yaw);
    dir.y = cos(pitch) * sin(yaw);
    dir.z = -sin(pitch);
    return dir;
}

namespace ESP {
    void DrawViewDirection(const EntityData& entity, const Matrix4x4& viewMatrix,
        int screenWidth, int screenHeight, const float* color,
        float length, const Vector3& viewAngles) {
        if (!entity.bonesValid) return;

        Vector3 start = entity.headPos;
        Vector2 screenStart;
        if (!WorldToScreen(start, screenStart, viewMatrix, screenWidth, screenHeight))
            return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        const float fovHalf = 45.0f;
        const int   segments = 20;
        const float coneLength = length;

        ImVec4 baseColor(color[0], color[1], color[2], color[3]);

        // Rita en solfjäder av linjer med avtagande alpha mot kanterna
        const int numLines = 21;
        float maxAlpha = baseColor.w;
        float minAlpha = baseColor.w * 0.25f;

        for (int i = 0; i < numLines; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(numLines - 1);
            float yawOffset = -fovHalf + t * 2.0f * fovHalf;
            float centerDist = std::abs(t - 0.5f) * 2.0f;
            float alpha = maxAlpha - (maxAlpha - minAlpha) * centerDist;

            Vector3 adjustedAngle = viewAngles;
            adjustedAngle.y += yawOffset;
            Vector3 dir = AngleToDirection(adjustedAngle);
            Vector3 end = start + dir * coneLength;

            Vector2 screenEnd;
            if (WorldToScreen(end, screenEnd, viewMatrix, screenWidth, screenHeight)) {
                ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(baseColor.x, baseColor.y, baseColor.z, alpha));
                float thickness = 1.2f + 0.8f * (1.0f - centerDist);
                draw->AddLine(ImVec2(screenStart.x, screenStart.y),
                    ImVec2(screenEnd.x, screenEnd.y), lineColor, thickness);
            }
        }

        // Central linje (extra tydlig)
        Vector3 centerDir = AngleToDirection(viewAngles);
        Vector3 centerEnd = start + centerDir * coneLength;
        Vector2 screenCenterEnd;
        if (WorldToScreen(centerEnd, screenCenterEnd, viewMatrix, screenWidth, screenHeight)) {
            ImU32 centerCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(baseColor.x, baseColor.y, baseColor.z, baseColor.w));
            draw->AddLine(ImVec2(screenStart.x, screenStart.y),
                ImVec2(screenCenterEnd.x, screenCenterEnd.y), centerCol, 2.5f);
            draw->AddCircleFilled(ImVec2(screenCenterEnd.x, screenCenterEnd.y), 3.0f,
                centerCol);
        }

        // Kantlinjer (vänster och höger) med hög opacitet
        for (float sign : {-1.0f, 1.0f}) {
            Vector3 edgeAngle = viewAngles;
            edgeAngle.y += sign * fovHalf;
            Vector3 edgeDir = AngleToDirection(edgeAngle);
            Vector3 edgeEnd = start + edgeDir * coneLength;
            Vector2 screenEdgeEnd;
            if (WorldToScreen(edgeEnd, screenEdgeEnd, viewMatrix, screenWidth, screenHeight)) {
                ImU32 edgeCol = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(baseColor.x, baseColor.y, baseColor.z, baseColor.w * 0.9f));
                draw->AddLine(ImVec2(screenStart.x, screenStart.y),
                    ImVec2(screenEdgeEnd.x, screenEdgeEnd.y), edgeCol, 1.8f);
            }
        }
    }
}