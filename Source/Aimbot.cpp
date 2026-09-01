#include "Aimbot.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include "SDK.h"
#include "CallStack-Spoofer.h"
#include <Windows.h>
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

namespace Aimbot {

    static uintptr_t s_lastTarget = 0;

    static bool IsVisible(uintptr_t targetPawn) {
        if (!targetPawn) return false;
        return mem.Read<bool>(targetPawn + offsets::csPawn::m_entitySpottedState + 0x8);
    }

    static void GetAimBones(bool* outBones, int hitboxMask) {
        for (int i = 0; i < BoneIndex::BONE_COUNT; i++)
            outBones[i] = false;
        if (hitboxMask & (1 << 0)) outBones[BoneIndex::HEAD] = true;
        if (hitboxMask & (1 << 1)) outBones[BoneIndex::NECK] = true;
        if (hitboxMask & (1 << 2)) outBones[BoneIndex::CHEST] = true;
        if (hitboxMask & (1 << 3)) outBones[BoneIndex::PELVIS] = true;
    }

    void Run(uintptr_t localPawn, uintptr_t localController, int screenWidth, int screenHeight) {
        SPOOF_FUNC;

        if (!config.bAimbot) {
            s_lastTarget = 0;
            return;
        }

        // ---- Aim mode ----
        static bool toggleState = false;
        SHORT(WINAPI * pGetAsyncKeyState)(int) = &GetAsyncKeyState;

        if (config.aimMode == 1) { // Hold
            if (!(SPOOF_CALL(pGetAsyncKeyState)(config.aimKey) & 0x8000))
                return;
        }
        else if (config.aimMode == 2) { // Toggle
            static bool lastKeyState = false;
            bool currentKeyState = (SPOOF_CALL(pGetAsyncKeyState)(config.aimKey) & 0x8000) != 0;
            if (currentKeyState && !lastKeyState)
                toggleState = !toggleState;
            lastKeyState = currentKeyState;
            if (!toggleState)
                return;
        }

        // ---- Local data ----
        Vector3 viewAngles = mem.Read<Vector3>(mem.client + offsets::client::dwViewAngles);
        NormalizeAngles(viewAngles);

        uintptr_t sceneNode = mem.Read<uintptr_t>(localPawn + offsets::entity::m_pGameSceneNode);
        if (!sceneNode) return;
        Vector3 localOrigin = mem.Read<Vector3>(sceneNode + offsets::sceneNode::m_vecAbsOrigin);
        Vector3 viewOffset = mem.Read<Vector3>(localPawn + offsets::model::m_vecViewOffset);
        if (viewOffset.z < 1.0f || viewOffset.z > 100.0f)
            viewOffset = { 0.0f, 0.0f, 64.06f };
        Vector3 eyePos = localOrigin + viewOffset;
        uint8_t localTeam = mem.Read<uint8_t>(localPawn + offsets::entity::m_iTeamNum);

        // ---- Smooth value (with randomisation) ----
        float smooth = config.aimSmooth;
        if (config.bAimSmoothRandom) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            float min = (std::min)(config.aimSmoothMin, config.aimSmoothMax);
            float max = (std::max)(config.aimSmoothMin, config.aimSmoothMax);
            std::uniform_real_distribution<float> dist(min, max);
            smooth = dist(gen);
        }
        float divisor = 1.0f + smooth * 2.0f;

        // ---- Entity iteration ----
        uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
        if (!entityList) return;

        bool aimBones[BoneIndex::BONE_COUNT];
        GetAimBones(aimBones, config.hitboxMask);

        float bestAngleDiff = FLT_MAX;
        Vector3 bestAngle = {};

        for (int i = 1; i <= 64; i++) {
            uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i >> 9) + 16));
            if (!listEntry) continue;

            uintptr_t controller = mem.Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!controller || controller == localController) continue;

            uint32_t pawnHandle = mem.Read<uint32_t>(controller + offsets::controller::m_hPlayerPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;

            uintptr_t pawnEntry = mem.Read<uintptr_t>(entityList + (8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
            if (!pawnEntry) continue;

            uintptr_t pawn = mem.Read<uintptr_t>(pawnEntry + 112 * ((pawnHandle & 0x7FFF) & 0x1FF));
            if (!pawn || pawn == localPawn) continue;

            int health = mem.Read<int>(pawn + offsets::entity::m_iHealth);
            uint8_t team = mem.Read<uint8_t>(pawn + offsets::entity::m_iTeamNum);
            if (health <= 0 || health > 100) continue;
            if (team == localTeam) continue;

            uintptr_t pawnSceneNode = mem.Read<uintptr_t>(pawn + offsets::entity::m_pGameSceneNode);
            if (!pawnSceneNode) continue;
            if (mem.Read<bool>(pawnSceneNode + offsets::sceneNode::m_bDormant)) continue;

            uintptr_t boneMatrix = mem.Read<uintptr_t>(pawnSceneNode + offsets::skeleton::m_modelState + 0x80);
            if (!boneMatrix) continue;

            for (int b = 0; b < BoneIndex::BONE_COUNT; b++) {
                if (!aimBones[b]) continue;
                Vector3 bonePos = mem.Read<Vector3>(boneMatrix + b * 32);
                if (bonePos.IsZero()) continue;

                if (config.bVisibleCheck && !IsVisible(pawn))
                    continue;

                Vector3 angleTo = CalculateAngle(eyePos, bonePos);
                NormalizeAngles(angleTo);

                Vector3 delta = angleTo - viewAngles;
                NormalizeAngles(delta);
                float angleDiff = std::sqrt(delta.x * delta.x + delta.y * delta.y); // degrees

                if (angleDiff > config.aimFov) continue;

                if (angleDiff < bestAngleDiff) {
                    bestAngleDiff = angleDiff;
                    bestAngle = angleTo;
                }
            }
        }

        if (bestAngleDiff == FLT_MAX) {
            s_lastTarget = 0;
            return;
        }

        // ---- Apply aim with smoothing ----
        Vector3 finalAngle;
        if (divisor <= 1.0f) {
            finalAngle = bestAngle;
        }
        else {
            Vector3 delta = bestAngle - viewAngles;
            NormalizeAngles(delta);
            float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (dist < 0.05f) {
                finalAngle = bestAngle;
            }
            else {
                finalAngle = SmoothAngle(viewAngles, bestAngle, divisor);
                NormalizeAngles(finalAngle);
            }
        }

        // ---- Apply jitter (micro‑tremor) ----
        if (config.bAimJitter && config.aimJitterAmount > 0.0f) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            // Scale the slider value (0–2) to a tiny range (0–0.14°)
            float effectiveJitter = config.aimJitterAmount * 0.07f;
            std::uniform_real_distribution<float> dist(-effectiveJitter, effectiveJitter);
            finalAngle.x += dist(gen);
            finalAngle.y += dist(gen);
            NormalizeAngles(finalAngle);
        }

        finalAngle.x = std::clamp(finalAngle.x, -89.0f, 89.0f);
        mem.Write<Vector3>(mem.client + offsets::client::dwViewAngles, finalAngle);
    }
}