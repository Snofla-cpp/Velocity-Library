#pragma once
#include "SDK.h"

namespace ESP {
    void DrawViewDirection(const EntityData& entity, const Matrix4x4& viewMatrix,
        int screenWidth, int screenHeight, const float* color,
        float length, const Vector3& viewAngles);
}