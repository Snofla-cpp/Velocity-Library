#include "Name.h"
#include "imgui.h"

namespace ESP {
    void DrawName(const Vector2& top, const char* name, const float* color) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(name);
        float x = top.x - textSize.x * 0.5f;
        float y = top.y - textSize.y - 2.0f;
        draw->AddText(ImVec2(x + 1, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)), name);
        draw->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3])), name);
    }
}