#include "Distance.h"
#include "imgui.h"
#include <cstdio>

namespace ESP {
    void DrawDistance(const Vector2& bottom, float distance, const float* color) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fm", distance / 100.0f);
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        float x = bottom.x - textSize.x * 0.5f;
        float y = bottom.y + 6.0f;
        draw->AddText(ImVec2(x + 1, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)), buf);
        draw->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3])), buf);
    }
}