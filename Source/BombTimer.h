#pragma once
#include <cstdint>
#include <string>

namespace BombTimer {
    void Update();
    void Draw();
    bool IsActive();
    float GetTimeRemaining();
    std::string GetTimeString();
    bool IsDefusing();
    float GetDefuseProgress();
    std::string GetDefuserName();
}