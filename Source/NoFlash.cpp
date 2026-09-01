#include "NoFlash.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"

namespace Misc {
    void NoFlash(uintptr_t localPawn) {
        if (!config.bNoFlash) return;

        float flashAlpha = mem.Read<float>(localPawn + offsets::csPawn::m_flFlashMaxAlpha);
        if (flashAlpha > 0.0f) {
            mem.Write<float>(localPawn + offsets::csPawn::m_flFlashMaxAlpha, 0.0f);
        }
    }
}