#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "SDK.h"

namespace Radar
{
    // Called once per frame to read game data and update the radar.
    void Update();

    // Draw the radar window (call inside ImGui frame).
    void Draw();

    // Cleanup resources (textures).
    void Cleanup();
}