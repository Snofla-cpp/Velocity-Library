#include "HeadCircle.h"
#include "imgui.h"
#include "Config.h"
#include <cmath>   // for sqrtf

namespace ESP {

    void DrawHeadCircle(const EntityData& entity, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
        if (!entity.valid || !entity.alive)
            return;

        // Use head position (bones[HEAD] or fallback headPos)
        Vector3 headPos = entity.bonesValid ? entity.bones[BoneIndex::HEAD] : entity.headPos;
        Vector2 screenPos;
        if (!WorldToScreen(headPos, screenPos, viewMatrix, screenWidth, screenHeight))
            return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 color = ImGui::ColorConvertFloat4ToU32(
            ImVec4(config.espHeadCircleColor[0],
                config.espHeadCircleColor[1],
                config.espHeadCircleColor[2],
                config.espHeadCircleColor[3]));

        // Compute radius by projecting a small vertical offset from the head.
        // This gives correct perspective scaling with distance.
        const float headRadiusWorld = 8.0f; // approximate head radius in game units
        Vector3 offsetPos = headPos + Vector3(0.0f, 0.0f, headRadiusWorld);
        Vector2 screenOffset;
        float radius;

        if (WorldToScreen(offsetPos, screenOffset, viewMatrix, screenWidth, screenHeight)) {
            float dx = screenOffset.x - screenPos.x;
            float dy = screenOffset.y - screenPos.y;
            radius = sqrtf(dx * dx + dy * dy);
            // Clamp to reasonable bounds
            if (radius < 2.0f) radius = 2.0f;
            if (radius > 30.0f) radius = 30.0f;
        }
        else {
            // Fallback if offset projection fails
            radius = 6.0f;
        }

        // Increase segment count for larger radii to keep the circle smooth
        int segments = 12 + static_cast<int>(radius * 0.5f); // base 12 + extra per pixel
        if (segments < 12) segments = 12;
        if (segments > 64) segments = 64; // cap for performance

        draw->AddCircle(ImVec2(screenPos.x, screenPos.y), radius, color, segments, 2.0f);
    }

}