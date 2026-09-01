#include "Triggerbot.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <chrono>
#include <random>   // NEW

static void MouseDown() {
    SPOOF_FUNC;
    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    UINT(WINAPI * pSendInput)(UINT, LPINPUT, int) = &SendInput;
    SPOOF_CALL(pSendInput)(1, &inp, sizeof(INPUT));
}

static void MouseUp() {
    SPOOF_FUNC;
    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;

    UINT(WINAPI * pSendInput)(UINT, LPINPUT, int) = &SendInput;
    SPOOF_CALL(pSendInput)(1, &inp, sizeof(INPUT));
}

namespace Triggerbot {
    void Run(uintptr_t localPawn, uint8_t localTeam) {
        SPOOF_FUNC;

        static bool mouseHeld = false;
        static std::chrono::steady_clock::time_point reactionStart;
        static bool reactionActive = false;
        static bool toggleState = false;
        static std::chrono::steady_clock::time_point lastShotTime;
        static bool cooldownActive = false;

        auto release = [&]() {
            if (mouseHeld) {
                MouseUp();
                mouseHeld = false;
            }
            reactionActive = false;
            };

        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;
        if (!config.bTriggerbot) {
            release();
            cooldownActive = false;
            return;
        }

        // ---- Mode handling ----
        if (config.triggerMode == 1) { // Hold
            if (!(SPOOF_CALL(pGetAsyncKeyState)(config.triggerKey) & 0x8000)) {
                release();
                cooldownActive = false;
                return;
            }
        }
        else if (config.triggerMode == 2) { // Toggle
            static bool lastKeyState = false;
            bool currentKeyState = (SPOOF_CALL(pGetAsyncKeyState)(config.triggerKey) & 0x8000) != 0;
            if (currentKeyState && !lastKeyState)
                toggleState = !toggleState;
            lastKeyState = currentKeyState;
            if (!toggleState) {
                release();
                cooldownActive = false;
                return;
            }
        }

        // ---- Check crosshair ----
        int crosshairEntityId = mem.Read<int>(localPawn + offsets::csPawn::m_iIDEntIndex);
        if (crosshairEntityId <= 0) {
            release();
            cooldownActive = false;
            return;
        }

        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) { release(); cooldownActive = false; return; }

        uintptr_t listEntry = mem.Read<uintptr_t>(
            entityList + (8 * ((crosshairEntityId & 0x7FFF) >> 9) + 16)
        );
        if (!listEntry) { release(); cooldownActive = false; return; }

        uintptr_t entity = mem.Read<uintptr_t>(listEntry + 112 * ((crosshairEntityId & 0x7FFF) & 0x1FF));
        if (!entity) { release(); cooldownActive = false; return; }

        int health = mem.Read<int>(entity + offsets::entity::m_iHealth);
        uint8_t team = mem.Read<uint8_t>(entity + offsets::entity::m_iTeamNum);

        if (health <= 0 || team == localTeam) {
            release();
            cooldownActive = false;
            return;
        }

        // ---- Determine reaction time and cooldown (with randomisation) ----
        int reactionTime = config.triggerReactionTime;
        if (config.bTriggerReactionRandom) {
            int min = (std::min)(config.triggerReactionMin, config.triggerReactionMax);
            int max = (std::max)(config.triggerReactionMin, config.triggerReactionMax);
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(min, max);
            reactionTime = dist(gen);
        }

        int shotCooldown = config.triggerShotCooldown;
        if (config.bTriggerCooldownRandom) {
            int min = (std::min)(config.triggerCooldownMin, config.triggerCooldownMax);
            int max = (std::max)(config.triggerCooldownMin, config.triggerCooldownMax);
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(min, max);
            shotCooldown = dist(gen);
        }

        // ---- Check cooldown (delay between shots) ----
        auto now = std::chrono::steady_clock::now();
        if (cooldownActive) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShotTime).count();
            if (elapsed < shotCooldown) {
                release();
                return;
            }
            cooldownActive = false;
        }

        // ---- Reaction time (delay before first shot) ----
        if (reactionTime > 0) {
            if (!reactionActive) {
                reactionStart = now;
                reactionActive = true;
                return;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - reactionStart).count();
            if (elapsed < reactionTime) {
                return;
            }
            reactionActive = false;
        }

        // ---- Fire ----
        if (!mouseHeld) {
            MouseDown();
            mouseHeld = true;
        }

        // ---- Record shot time and activate cooldown ----
        lastShotTime = std::chrono::steady_clock::now();
        cooldownActive = true;
        reactionActive = false;
    }
}