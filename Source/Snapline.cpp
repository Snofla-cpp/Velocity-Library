#include "Snapline.h"
#include "imgui.h"

namespace ESP {
    void DrawSnapline(const Vector2& bottom, int screenW, int screenH, const float* color) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
        draw->AddLine(ImVec2(screenW * 0.5f, static_cast<float>(screenH)), ImVec2(bottom.x, bottom.y), col, 1.0f);
    }
}