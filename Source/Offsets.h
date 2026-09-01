// Generated using https://github.com/a2x/cs2-dumper
// 2026-08-30 12:44:20.984104200 UTC

#pragma once

#include <cstddef>
#include <cstdint>

namespace offsets {
    // offsets.hpp:
    constexpr std::ptrdiff_t dwLocalPlayerController = 0x23A0F30;

    constexpr std::ptrdiff_t m_nSubclassID = 0x380;

    constexpr std::ptrdiff_t m_szName = 0x720;

    constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x23C6268;

    // client_dll.hpp:
    constexpr std::ptrdiff_t m_sSanitizedPlayerName = 0x868;

    // Update from https://github.com/sezzyaep/CS2-OFFSETS/blob/main or https://github.com/a2x/cs2-dumper/tree/main/output!
    // Run the offset dumper after each game update to keep these up to date. Once you run it, it will output a bunch of files. Open up client_dll.hpp and offsets.hpp and copy the relevant namespaces and offsets into the corresponding namespaces below. Make sure to update the comments with the new dump date and any relevant notes about changes in the offsets (e.g. if an offset was added, removed, or changed significantly). This will ensure that the rest of the codebase can rely on these offsets to read game memory correctly.

    // Module: offsets.hpp
    namespace client {
        constexpr std::ptrdiff_t dwEntityList = 0x2571220;          // updated from 0x2571230
        constexpr std::ptrdiff_t dwLocalPlayerController = 0x23A0F30;
        constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x23C6268;
        constexpr std::ptrdiff_t dwPlantedC4 = 0x2390A18;
        constexpr std::ptrdiff_t dwViewMatrix = 0x23CB830;
        constexpr std::ptrdiff_t dwViewAngles = 0x23DC2F8;          // updated from 0x23DC308
        constexpr std::ptrdiff_t dwGlobalVars = 0x20AF5F0;
        constexpr std::ptrdiff_t dwGameRules = 0x23C5D28;
    }

    // Module: offsets.hpp
    namespace engine {
        constexpr std::ptrdiff_t dwBuildNumber = 0x60F594;
    }

    namespace C_C4 {
        constexpr std::ptrdiff_t m_bStartedArming = 0x1CE8;      // updated from 0x1099
        constexpr std::ptrdiff_t m_fArmedTime = 0x1CEC;          // updated from 0x109C
        constexpr std::ptrdiff_t m_bIsPlantingViaUse = 0x1CF1;   // updated from 0x10A1
        constexpr std::ptrdiff_t m_bBombPlanted = 0x1D1B;        // updated from 0x10CB
    }

    namespace C_PlantedC4 {
        constexpr std::ptrdiff_t m_bBombTicking = 0x11A0; // bool
        constexpr std::ptrdiff_t m_nBombSite = 0x11A4; // int32
        constexpr std::ptrdiff_t m_nSourceSoundscapeHash = 0x11A8; // int32
        constexpr std::ptrdiff_t m_entitySpottedState = 0x11B0; // EntitySpottedState_t
        constexpr std::ptrdiff_t m_flNextGlow = 0x11C8; // GameTime_t
        constexpr std::ptrdiff_t m_flNextBeep = 0x11CC; // GameTime_t
        constexpr std::ptrdiff_t m_flC4Blow = 0x11D0; // GameTime_t
        constexpr std::ptrdiff_t m_bCannotBeDefused = 0x11D4; // bool
        constexpr std::ptrdiff_t m_bHasExploded = 0x11D5; // bool
        constexpr std::ptrdiff_t m_flTimerLength = 0x11D8; // float32
        constexpr std::ptrdiff_t m_bBeingDefused = 0x11DC; // bool
        constexpr std::ptrdiff_t m_bTriggerWarning = 0x11E0; // float32
        constexpr std::ptrdiff_t m_bExplodeWarning = 0x11E4; // float32
        constexpr std::ptrdiff_t m_bC4Activated = 0x11E8; // bool
        constexpr std::ptrdiff_t m_bTenSecWarning = 0x11E9; // bool
        constexpr std::ptrdiff_t m_flDefuseLength = 0x11EC; // float32
        constexpr std::ptrdiff_t m_flDefuseCountDown = 0x11F0; // GameTime_t
        constexpr std::ptrdiff_t m_bBombDefused = 0x11F4; // bool
        constexpr std::ptrdiff_t m_hBombDefuser = 0x11F8; // CHandle<C_CSPlayerPawn>
        constexpr std::ptrdiff_t m_AttributeManager = 0x1200; // C_AttributeContainer
        constexpr std::ptrdiff_t m_hDefuserMultimeter = 0x16D0; // CHandle<C_Multimeter>
        constexpr std::ptrdiff_t m_flNextRadarFlashTime = 0x16D4; // GameTime_t
        constexpr std::ptrdiff_t m_bRadarFlash = 0x16D8; // bool
        constexpr std::ptrdiff_t m_pBombDefuser = 0x16DC; // CHandle<C_CSPlayerPawn>
        constexpr std::ptrdiff_t m_fLastDefuseTime = 0x16E0; // GameTime_t
        constexpr std::ptrdiff_t m_pPredictionOwner = 0x16E8; // CBasePlayerController*
        constexpr std::ptrdiff_t m_vecC4ExplodeSpectatePos = 0x16F0; // VectorWS
        constexpr std::ptrdiff_t m_vecC4ExplodeSpectateAng = 0x16FC; // QAngle
        constexpr std::ptrdiff_t m_flC4ExplodeSpectateDuration = 0x1708; // float32
    }

    // Module: client_dll.hpp
    namespace entity {
        constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
        constexpr std::ptrdiff_t m_iHealth = 0x34C;
        constexpr std::ptrdiff_t m_iMaxHealth = 0x348;
        constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;
        constexpr std::ptrdiff_t m_lifeState = 0x354;
        constexpr std::ptrdiff_t m_fFlags = 0x3F4;
        constexpr std::ptrdiff_t m_AttributeManager = 0x11A8;
    }

    // Module: client_dll.hpp
    namespace sceneNode {
        constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
        constexpr std::ptrdiff_t m_bDormant = 0x103;
    }

    // Module: client_dll.hpp
    namespace skeleton {
        constexpr std::ptrdiff_t m_modelState = 0x140;
    }

    // Module: client_dll.hpp
    namespace csPawn {
        constexpr std::ptrdiff_t m_ArmorValue = 0x1CA4;
        constexpr std::ptrdiff_t m_bIsScoped = 0x1C78;
        constexpr std::ptrdiff_t m_flC4Blow = 0x11D0;
        constexpr std::ptrdiff_t m_flFlashMaxAlpha = 0x1424;
        constexpr std::ptrdiff_t m_angEyeAngles = 0x3350;
        constexpr std::ptrdiff_t m_bSpottedByMask = 0xC;
        constexpr std::ptrdiff_t m_entitySpottedState = 0x1C60;
        constexpr std::ptrdiff_t m_iIDEntIndex = 0x342C;
        constexpr std::ptrdiff_t m_pCameraServices = 0x1240;
        constexpr std::ptrdiff_t m_flFOV = 0x290;
        constexpr std::ptrdiff_t m_bIsPlantingViaUse = 0x10A1;
        constexpr std::ptrdiff_t m_bBombPlanted = 0x10CB;
    }

    namespace cameraServices {
        constexpr std::ptrdiff_t m_iFOV = 0x290;      // uint32_t (degrees)
        constexpr std::ptrdiff_t m_iFOVStart = 0x294; // (optional)
        constexpr std::ptrdiff_t m_flFOVTime = 0x298; // (optional)
        constexpr std::ptrdiff_t m_flFOVRate = 0x29C; // (optional)
        constexpr std::ptrdiff_t m_hZoomOwner = 0x2A0;
        constexpr std::ptrdiff_t m_flLastShotFOV = 0x2A4;
    }

    // Module: client_dll.hpp
    namespace controller {
        constexpr std::ptrdiff_t m_hPlayerPawn = 0x914;
        constexpr std::ptrdiff_t m_iszPlayerName = 0x6F4;
        constexpr std::ptrdiff_t m_iPawnHealth = 0x920;
        constexpr std::ptrdiff_t m_iPawnArmor = 0x924;
        constexpr std::ptrdiff_t m_bPawnHasHelmet = 0x929;
    }

    namespace C_BasePlayerPawn {
        constexpr std::ptrdiff_t m_pMovementServices = 0x1248;
    }

    // Module: client_dll.hpp
    namespace playerPawn {
        constexpr std::ptrdiff_t m_pWeaponServices = 0x1208;
    }

    // Module: client_dll.hpp
    namespace weaponServices {
        constexpr std::ptrdiff_t m_hActiveWeapon = 0x60;
    }

    // Module: client_dll.hpp
    namespace weaponEntity {
        constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
    }

    namespace buttons {
        constexpr std::ptrdiff_t jump = 0x20B3E00;
    }



    // Module: client_dll.hpp
    namespace attributeManager {
        constexpr std::ptrdiff_t m_Item = 0x50;
    }

    // Module: client_dll.hpp
    namespace econItemView {
        constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
    }

    namespace model {
        constexpr std::ptrdiff_t m_Glow = 0xDE0;   // CGlowProperty (C_BaseModelEntity)
        constexpr std::ptrdiff_t m_vecViewOffset = 0xE78;   // updated from 0xE70 to 0xE78
    }

    namespace glow {
        constexpr std::ptrdiff_t m_glowColorOverride = 0x40;  // Color (CGlowProperty)
        constexpr std::ptrdiff_t m_bGlowing = 0x51;           // bool (CGlowProperty)
    }
}