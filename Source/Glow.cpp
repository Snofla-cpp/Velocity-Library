#include "Glow.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include <algorithm>

namespace Glow {
    // Cache for each entity index (1–63)
    struct CachedGlow {
        bool hasValidState = false;  // whether we have a previously valid state
        DWORD color = 0;
        bool glowing = false;
    };
    static CachedGlow cache[64];

    void Run(uintptr_t localController) {
        if (!config.bGlow) return;
        if (!localController) return;

        uint8_t localTeam = mem.Read<uint8_t>(localController + offsets::entity::m_iTeamNum);
        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) return;

        for (int i = 1; i < 64; ++i) {
            CachedGlow& cached = cache[i];

            // ---- Read controller entry ----
            uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9)) + 16);
            if (!listEntry) {
                // If we can't even read the list entry, keep previous state (skip writing)
                continue;
            }

            uintptr_t player = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!player || player < 0x10000) {
                // Invalid controller, keep previous state
                continue;
            }

            uint8_t playerTeam = mem.Read<uint8_t>(player + offsets::entity::m_iTeamNum);

            // ---- Team filtering ----
            if (config.glowType != 1) {
                if (config.bGlowTeamCheck && playerTeam == localTeam)
                    continue;
            }

            // ---- Read pawn handle ----
            uint32_t pawnHandle = mem.Read<uint32_t>(player + offsets::controller::m_hPlayerPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) {
                // Can't get pawn, keep previous state
                continue;
            }

            // ---- Read pawn entry ----
            uintptr_t listEntry2 = mem.Read<uintptr_t>(entityList + 8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
            if (!listEntry2) {
                continue;
            }

            uintptr_t playerCsPawn = mem.Read<uintptr_t>(listEntry2 + 112 * (pawnHandle & 0x1FF));
            if (!playerCsPawn || playerCsPawn < 0x10000) {
                continue;
            }

            // ---- Read health and life state ----
            int health = mem.Read<int>(playerCsPawn + offsets::entity::m_iHealth);
            uint8_t lifeState = mem.Read<uint8_t>(playerCsPawn + offsets::entity::m_lifeState);

            // ---- Read dormancy ----
            uintptr_t sceneNode = mem.Read<uintptr_t>(playerCsPawn + offsets::entity::m_pGameSceneNode);
            if (!sceneNode || sceneNode < 0x10000) {
                // Can't get scene node, keep previous state
                continue;
            }
            bool dormant = mem.Read<bool>(sceneNode + offsets::sceneNode::m_bDormant);

            // ---- Determine if entity is valid for glow ----
            bool isValid = (health > 0 && health <= 100 && lifeState == 0 && !dormant);

            if (isValid) {
                // ---- Compute color based on type ----
                float r, g, b, a;
                switch (config.glowType) {
                case 0: // Regular
                    r = config.glowColor[0];
                    g = config.glowColor[1];
                    b = config.glowColor[2];
                    a = config.glowColor[3];
                    break;
                case 1: // Team Based
                    if (playerTeam == localTeam) {
                        r = config.glowTeamColor[0];
                        g = config.glowTeamColor[1];
                        b = config.glowTeamColor[2];
                        a = config.glowTeamColor[3];
                    }
                    else {
                        r = config.glowEnemyColor[0];
                        g = config.glowEnemyColor[1];
                        b = config.glowEnemyColor[2];
                        a = config.glowEnemyColor[3];
                    }
                    break;
                case 2: // Health Based
                default: {
                    float healthPercent = std::clamp(health / 100.0f, 0.0f, 1.0f);
                    r = 1.0f - healthPercent;
                    g = healthPercent;
                    b = 0.0f;
                    a = 1.0f;
                    break;
                }
                }

                DWORD colorArgb = (
                    (static_cast<DWORD>(a * 255) << 24) |
                    (static_cast<DWORD>(b * 255) << 16) |
                    (static_cast<DWORD>(g * 255) << 8) |
                    static_cast<DWORD>(r * 255)
                    );

                // ---- Apply glow ----
                uintptr_t glowOffset = playerCsPawn + offsets::model::m_Glow;
                mem.Write<DWORD>(glowOffset + offsets::glow::m_glowColorOverride, colorArgb);
                mem.Write<bool>(glowOffset + offsets::glow::m_bGlowing, true);

                // Update cache
                cached.hasValidState = true;
                cached.color = colorArgb;
                cached.glowing = true;
            }
            else {
                // Entity is not valid – turn off glow
                uintptr_t glowOffset = playerCsPawn + offsets::model::m_Glow;
                mem.Write<bool>(glowOffset + offsets::glow::m_bGlowing, false);

                // Update cache to reflect off state
                cached.hasValidState = true;
                cached.glowing = false;
            }
        }
    }
}