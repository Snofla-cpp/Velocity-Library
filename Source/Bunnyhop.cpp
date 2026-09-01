#include "Bunnyhop.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>

namespace Misc {
    void Bhop(uintptr_t localPawn) {
        SPOOF_FUNC;
        if (!config.bBhop) return;

        // FindWindowA is called normally (not spoofed) to avoid template issues
        HWND hwnd_cs2 = FindWindowA(NULL, "Counter-Strike 2");
        if (hwnd_cs2 == NULL)
            hwnd_cs2 = FindWindowA(NULL, "Counter-Strike 2");

        // Only spoof GetAsyncKeyState
        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;
        bool spacePressed = (SPOOF_CALL(pGetAsyncKeyState)(VK_SPACE) & 0x8000) != 0;
        uint32_t flags = mem.Read<uint32_t>(localPawn + offsets::entity::m_fFlags);
        bool isInAir = (flags & 1) != 0;

        if (spacePressed && isInAir) {
            SendMessage(hwnd_cs2, WM_KEYUP, VK_SPACE, 0);
            SendMessage(hwnd_cs2, WM_KEYDOWN, VK_SPACE, 0);
        }
        else if (spacePressed && !isInAir) {
            SendMessage(hwnd_cs2, WM_KEYUP, VK_SPACE, 0);
        }
        else if (!spacePressed) {
            SendMessage(hwnd_cs2, WM_KEYUP, VK_SPACE, 0);
        }
    }
}