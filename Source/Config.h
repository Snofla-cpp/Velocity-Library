#pragma once
#include <string>
#include <vector>

struct Config {
    bool show_menu = true;

    // ESP master toggle
    bool bEsp = false;
    bool bEspTeamCheck = false;

    // Box
    bool bEspBox = false;
    int boxType = 0;
    float boxThickness = 1.0f;
    bool bBoxFill = false;
    float espBoxColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float espFillColor[4] = { 1.0f, 1.0f, 1.0f, 0.3f };

    // Skeleton
    bool bEspSkeleton = false;
    float espSkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Name
    bool bEspName = false;
    float espNameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Health Bar
    bool bEspHealthBar = false;
    float espHealthColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

    // Armor Bar
    bool bEspArmorBar = false;
    float espArmorColor[4] = { 0.0f, 0.5f, 1.0f, 1.0f };

    // Head Circle
    bool bEspHeadCircle = false;
    float espHeadCircleColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

    // Distance
    bool bEspDistance = false;
    float espDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Snapline
    bool bEspSnapline = false;
    float espSnaplineColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // View Direction (Line of Sight)
    bool bEspViewDirection = false;
    float espViewDirectionColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    float espViewDirectionLength = 100.0f;

    // ---- HeadShot Line ----
    bool bHeadShotLine = false;
    float headShotColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Glow
    bool bGlow = false;
    int glowType = 0;
    bool bGlowTeamCheck = false;
    float glowColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float glowTeamColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float glowEnemyColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    // FOV
    bool bFovChanger = false;
    float fovValue = 90.0f;

    // Weapon
    bool bEspWeapon = false;
    float espWeaponColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Radar
    bool bRadar = false;
    float radarSize = 300.f;

    // ---- Flags (NEW) ----
    bool bFlags = false;
    int flagsMask = 0;                  // bit 0 = Bomb
    float espFlagsColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

    // Misc
    bool bBhop = false;
    bool bNoFlash = false;
    bool bStreamproof = true;
    int menuKey = 0x2D;
    bool bBombTimer = false;      // <-- ADD THIS

    // Aimbot
    bool bAimbot = false;
    int aimMode = 0;
    int aimKey = 0x01;
    int hitboxMask = 0;
    float aimFov = 30.0f;
    bool bDrawFov = false;
    float aimFovColor[4] = { 0.0f, 0.78f, 1.0f, 0.63f };
    float aimSmooth = 50.0f;
    bool bAimSmoothRandom = false;
    float aimSmoothMin = 0.0f;
    float aimSmoothMax = 100.0f;
    bool bAimJitter = false;
    float aimJitterAmount = 0.2f;
    bool bVisibleCheck = false;

    // Triggerbot
    bool bTriggerbot = false;
    int triggerMode = 0;
    int triggerKey = 0x12;
    int triggerReactionTime = 10;
    int triggerShotCooldown = 50;
    bool bTriggerReactionRandom = false;   // NEW
    int triggerReactionMin = 0;            // NEW
    int triggerReactionMax = 200;          // NEW
    bool bTriggerCooldownRandom = false;   // NEW
    int triggerCooldownMin = 0;            // NEW
    int triggerCooldownMax = 500;          // NEW

    static std::vector<std::string> GetConfigList();
    static bool LoadConfig(const std::string& filename);
    static bool SaveConfig(const std::string& filename);
    static bool CreateConfig(const std::string& filename);
    static bool DeleteConfig(const std::string& filename);
    static bool RenameConfig(const std::string& oldName, const std::string& newName);

    void SetDefaults();
};

extern Config config;