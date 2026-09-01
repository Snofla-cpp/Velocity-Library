#include "FOV.h"
#include "Config.h"
#include "Offsets.h"
#include "Memory.h"
#include <thread>
#include <atomic>
#include <Windows.h>

namespace FOV {

    static std::atomic<bool> s_running{ false };
    static std::thread s_thread;
    static std::atomic<bool> s_shouldStop{ false };

    // Core write routine – only writes if FOV changer is enabled
    static void DoWrite(uintptr_t localPawn) {
        if (!config.bFovChanger) return;      // ← OFF → do nothing
        if (!localPawn) return;

        uintptr_t cameraServices = mem.Read<uintptr_t>(localPawn + offsets::csPawn::m_pCameraServices);
        if (!cameraServices) return;

        uint32_t targetFov = static_cast<uint32_t>(config.fovValue);
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_iFOV, targetFov);
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_iFOVStart, targetFov);
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flFOVTime, -1e9f);
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flFOVRate, 0.0f);
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_hZoomOwner, 0);
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flLastShotFOV, static_cast<float>(targetFov));
    }

    // Background thread – always runs, but DoWrite will skip when disabled
    static void FOVThread() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (!s_shouldStop.load()) {
            uintptr_t localPawn = mem.Read<uintptr_t>(mem.client + offsets::client::dwLocalPlayerPawn);
            for (int i = 0; i < 100; ++i) {
                DoWrite(localPawn);
            }
            std::this_thread::yield();
        }
    }

    void Start() {
        if (s_running.exchange(true)) return;
        s_shouldStop.store(false);
        s_thread = std::thread(FOVThread);
    }

    void Stop() {
        if (!s_running.exchange(false)) return;
        s_shouldStop.store(true);
        if (s_thread.joinable()) s_thread.join();
    }

    void WriteNow(uintptr_t localPawn) {
        DoWrite(localPawn);
    }

    void Reset(uintptr_t localPawn) {
        if (!localPawn) return;
        uintptr_t cameraServices = mem.Read<uintptr_t>(localPawn + offsets::csPawn::m_pCameraServices);
        if (!cameraServices) return;
        const uint32_t defaultFov = 90;
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_iFOV, defaultFov);
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_iFOVStart, defaultFov);
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flFOVTime, -1e9f);   // <-- changed here
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flFOVRate, 0.0f);
        mem.Write<uint32_t>(cameraServices + offsets::cameraServices::m_hZoomOwner, 0);
        mem.Write<float>(cameraServices + offsets::cameraServices::m_flLastShotFOV, static_cast<float>(defaultFov));
    }
}