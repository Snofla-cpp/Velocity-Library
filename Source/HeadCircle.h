#pragma once
#include "SDK.h"          // for EntityData, Matrix4x4, Vector2

namespace ESP {
    void DrawHeadCircle(const EntityData& entity, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight);
}
