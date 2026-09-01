#pragma once
#include "SDK.h"

namespace ESP {
    void DrawBox(const Vector2& top, const Vector2& bottom,
        const float* outlineColor, const float* fillColor,
        int type, float thickness, bool fill);
}