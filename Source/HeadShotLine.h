#pragma once
#include "SDK.h"
#include <cstdint>

namespace HeadShotLine {
    // Draw a horizontal line indicating headshot height at a fixed reference distance.
    void Draw(uintptr_t localPawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight, const float* color);
}