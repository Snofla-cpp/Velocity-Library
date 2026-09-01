#include "HealthBar.h"
#include "imgui.h"
#include <cstdio>

namespace ESP {
    void DrawHealthBar(const Vector2& top, const Vector2& bottom, int health, const float* color) {
        float height = bottom.y - top.y;
        float width = height / 2.4f;
        float barHeight = height * (health / 100.0f);

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.6f));
        ImU32 hpColor = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));

        float barLeft = top.x - width * 0.5f - 6.0f;
        float barRight = barLeft + 3.0f;

        draw->AddRectFilled(ImVec2(barLeft - 1, top.y - 1), ImVec2(barRight + 1, bottom.y + 1), bgColor);
        draw->AddRectFilled(ImVec2(barLeft, bottom.y - barHeight), ImVec2(barRight, bottom.y), hpColor);

        if (health < 100) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", health);
            ImVec2 textSize = ImGui::CalcTextSize(buf);
            float textY = bottom.y - barHeight - textSize.y * 0.5f;
            textY = (std::max)(textY, top.y - textSize.y);
            draw->AddText(ImVec2(barLeft - textSize.x - 2, textY),
                ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3])), buf);
        }
    }
}