#include "Skeleton.h"
#include "imgui.h"

namespace ESP {

    // Updated bone connections from the new struct
    static const int connections[][2] = {
        // Spine
        { BoneIndex::PELVIS, BoneIndex::SPINE1 },
        { BoneIndex::SPINE1, BoneIndex::SPINE2 },
        { BoneIndex::SPINE2, BoneIndex::CHEST },
        { BoneIndex::CHEST, BoneIndex::NECK },
        // Left arm
        { BoneIndex::NECK, BoneIndex::SHOULDER_L },
        { BoneIndex::SHOULDER_L, BoneIndex::ELBOW_L },
        { BoneIndex::ELBOW_L, BoneIndex::HAND_L },
        // Right arm
        { BoneIndex::NECK, BoneIndex::SHOULDER_R },
        { BoneIndex::SHOULDER_R, BoneIndex::ELBOW_R },
        { BoneIndex::ELBOW_R, BoneIndex::HAND_R },
        // Left leg
        { BoneIndex::PELVIS, BoneIndex::HIP_L },
        { BoneIndex::HIP_L, BoneIndex::KNEE_L },
        { BoneIndex::KNEE_L, BoneIndex::FOOT_HEEL_L },
        // Right leg
        { BoneIndex::PELVIS, BoneIndex::HIP_R },
        { BoneIndex::HIP_R, BoneIndex::KNEE_R },
        { BoneIndex::KNEE_R, BoneIndex::FOOT_HEEL_R },
    };

    void DrawSkeleton(const Vector3* bones, const Matrix4x4& vm, int w, int h, const float* color) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));

        for (const auto& c : connections) {
            Vector2 from, to;
            if (WorldToScreen(bones[c[0]], from, vm, w, h) &&
                WorldToScreen(bones[c[1]], to, vm, w, h)) {
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), col, 1.5f);
            }
        }
    }
}