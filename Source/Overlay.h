#pragma once
#include <d3d11.h>
#include <array>
#include <mutex>
#include "SDK.h"   // Vector3, Matrix4x4, BoneIndex

// ------------------------------
//  Cachad entitetsdata
// ------------------------------
struct CachedEntity {
    bool valid = false;
    uintptr_t controller = 0;
    uintptr_t pawn = 0;
    int health = 0;
    int armor = 0;
    int team = 0;
    float distance = 0.0f;
    Vector3 origin{};
    Vector3 headPos{};
    Vector3 bones[BoneIndex::BONE_COUNT];
    bool bonesValid = false;
    char name[128] = {};
    char weaponName[32] = {};
    Vector3 viewAngles{};          // <-- för ViewDirection
};

// ------------------------------
//  Cache-klass (trådsäker)
// ------------------------------
class EntityCache {
public:
    static constexpr int MAX_PLAYERS = 64;
    void Update();   // anropas av bakgrundstråden
    const std::array<CachedEntity, MAX_PLAYERS>& Get() const; // read-only

private:
    std::array<CachedEntity, MAX_PLAYERS> m_entities{};
    mutable std::mutex m_mutex;
};

// ------------------------------
//  Overlay-namnrymd
// ------------------------------
namespace Overlay {
    bool Create();
    void Destroy();
    void Run();
    bool IsRunning();
}

extern ID3D11Device* g_pd3dDevice;
extern EntityCache g_entityCache;   // global instans