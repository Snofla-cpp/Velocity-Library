#include "ArmorBar.h"
#include "imgui.h"

namespace ESP {
    void DrawArmorBar(const Vector2& top, const Vector2& bottom, int armor, const float* color) {
        float height = bottom.y - top.y;
        float width = height / 2.4f;
        float barWidth = width * (armor / 100.0f);

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.6f));
        ImU32 armorColor = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));

        float barTop = bottom.y + 2.0f;
        float barBottom = barTop + 3.0f;
        float left = top.x - width * 0.5f;
        float right = top.x + width * 0.5f;

        draw->AddRectFilled(ImVec2(left - 1, barTop - 1), ImVec2(right + 1, barBottom + 1), bgColor);
        draw->AddRectFilled(ImVec2(left, barTop), ImVec2(left + barWidth, barBottom), armorColor);
    }

}
