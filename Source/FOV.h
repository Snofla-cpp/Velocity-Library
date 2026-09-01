#pragma once
#include <cstdint>

namespace FOV {
    void Start();              // start background thread
    void Stop();               // stop thread
    void Reset(uintptr_t localPawn); // restore default FOV
    void WriteNow(uintptr_t localPawn); // single immediate write (called each frame)
}