#include "Box.h"
#include "imgui.h"

namespace ESP {
    void DrawBox(const Vector2& top, const Vector2& bottom,
        const float* outlineColor, const float* fillColor,
        int type, float thickness, bool fill) {
        float height = bottom.y - top.y;
        float width = height / 2.4f;
        float left = top.x - width * 0.5f;
        float right = top.x + width * 0.5f;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(outlineColor[0], outlineColor[1], outlineColor[2], outlineColor[3]));
        ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1));
        ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(fillColor[0], fillColor[1], fillColor[2], fillColor[3]));

        // ---- Fill ----
        if (fill) {
            draw->AddRectFilled(ImVec2(left, top.y), ImVec2(right, bottom.y), fillCol);
        }

        if (type == 0) { // 2D full box – use AddRect for seamless border
            // Outer outline (black)
            draw->AddRect(ImVec2(left, top.y), ImVec2(right, bottom.y), outline, 0.0f, 0, thickness + 2.0f);
            // Inner colored line
            draw->AddRect(ImVec2(left, top.y), ImVec2(right, bottom.y), col, 0.0f, 0, thickness);
        }
        else { // Corner box – keep existing corner segments
            float cornerLen = width * 0.3f;

            // Helper to draw a line segment with outline
            auto DrawLine = [&](const ImVec2& p1, const ImVec2& p2, float thick) {
                draw->AddLine(p1, p2, outline, thick + 2.0f);
                draw->AddLine(p1, p2, col, thick);
                };

            // Top-left
            DrawLine(ImVec2(left, top.y), ImVec2(left + cornerLen, top.y), thickness);
            DrawLine(ImVec2(left, top.y), ImVec2(left, top.y + cornerLen), thickness);
            // Top-right
            DrawLine(ImVec2(right - cornerLen, top.y), ImVec2(right, top.y), thickness);
            DrawLine(ImVec2(right, top.y), ImVec2(right, top.y + cornerLen), thickness);
            // Bottom-left
            DrawLine(ImVec2(left, bottom.y - cornerLen), ImVec2(left, bottom.y), thickness);
            DrawLine(ImVec2(left, bottom.y), ImVec2(left + cornerLen, bottom.y), thickness);
            // Bottom-right
            DrawLine(ImVec2(right - cornerLen, bottom.y), ImVec2(right, bottom.y), thickness);
            DrawLine(ImVec2(right, bottom.y - cornerLen), ImVec2(right, bottom.y), thickness);
        }
    }
}