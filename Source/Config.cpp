#include "Config.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

Config config;

static std::string FloatArrayToString(const float arr[4]) {
    std::ostringstream oss;
    oss << arr[0] << ' ' << arr[1] << ' ' << arr[2] << ' ' << arr[3];
    return oss.str();
}

static void StringToFloatArray(const std::string& str, float arr[4]) {
    std::istringstream iss(str);
    iss >> arr[0] >> arr[1] >> arr[2] >> arr[3];
}

void Config::SetDefaults() {
    show_menu = true;
    bEsp = false;
    bEspTeamCheck = true;
    bEspBox = true;
    boxType = 0;
    boxThickness = 1.0f;
    bBoxFill = false;

    float defaultBox[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    memcpy(espBoxColor, defaultBox, sizeof(espBoxColor));

    float defaultFill[4] = { 1.0f, 1.0f, 1.0f, 0.3f };
    memcpy(espFillColor, defaultFill, sizeof(espFillColor));

    bEspSkeleton = false;
    float defaultWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    memcpy(espSkeletonColor, defaultWhite, sizeof(espSkeletonColor));

    bEspName = false;
    memcpy(espNameColor, defaultWhite, sizeof(espNameColor));

    bEspHealthBar = false;
    float defaultHealth[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    memcpy(espHealthColor, defaultHealth, sizeof(espHealthColor));

    bEspArmorBar = false;
    float defaultArmor[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    memcpy(espArmorColor, defaultArmor, sizeof(espArmorColor));

    bEspHeadCircle = false;
    float defaultHeadCircle[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    memcpy(espHeadCircleColor, defaultHeadCircle, sizeof(espHeadCircleColor));

    bEspDistance = false;
    memcpy(espDistanceColor, defaultWhite, sizeof(espDistanceColor));

    bEspSnapline = false;
    memcpy(espSnaplineColor, defaultWhite, sizeof(espSnaplineColor));

    bEspViewDirection = false;
    float defaultYellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    memcpy(espViewDirectionColor, defaultYellow, sizeof(espViewDirectionColor));
    espViewDirectionLength = 100.0f;

    bHeadShotLine = false;
    memcpy(headShotColor, defaultWhite, sizeof(headShotColor));

    // Glow
    bGlow = false;
    glowType = 0;
    bGlowTeamCheck = false;
    float defaultRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float defaultBlue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    memcpy(glowColor, defaultRed, sizeof(glowColor));
    memcpy(glowTeamColor, defaultBlue, sizeof(glowTeamColor));
    memcpy(glowEnemyColor, defaultRed, sizeof(glowEnemyColor));

    // Misc
    bBhop = false;
    bNoFlash = false;
    menuKey = 0x2D;
    bStreamproof = true;
    bBombTimer = false;      // <-- ADD THIS

    // Aimbot
    bAimbot = false;
    aimMode = 0;
    aimKey = 0x01;
    hitboxMask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
    aimFov = 30.0f;
    bDrawFov = false;
    aimSmooth = 50.0f;
    bAimSmoothRandom = false;
    aimSmoothMin = 0.0f;
    aimSmoothMax = 100.0f;
    bAimJitter = false;
    aimJitterAmount = 0.2f;
    bVisibleCheck = false;
    float defaultFovColor[4] = { 0.0f, 0.78f, 1.0f, 0.63f };
    memcpy(aimFovColor, defaultFovColor, sizeof(aimFovColor));

    // Triggerbot
    bTriggerbot = false;
    triggerMode = 0;
    triggerKey = 0x12;
    triggerReactionTime = 10;
    triggerShotCooldown = 50;
    bTriggerReactionRandom = false;
    triggerReactionMin = 0;
    triggerReactionMax = 200;
    bTriggerCooldownRandom = false;
    triggerCooldownMin = 0;
    triggerCooldownMax = 500;

    bFovChanger = false;
    fovValue = 90.0f;
    bRadar = false;
}

std::vector<std::string> Config::GetConfigList() {
    std::vector<std::string> files;
    const std::string configDir = "C:\\Velocity\\configs\\";
    if (!fs::exists(configDir)) {
        fs::create_directories(configDir);
        return files;
    }
    try {
        for (const auto& entry : fs::directory_iterator(configDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    catch (...) {}
    std::sort(files.begin(), files.end());
    return files;
}

bool Config::LoadConfig(const std::string& filename) {
    std::string path = "C:\\Velocity\\configs\\" + filename;
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key)) continue;
        char eq; iss >> eq;
        if (eq != '=') continue;
        std::getline(iss, value);
        while (!value.empty() && value[0] == ' ') value.erase(0, 1);

        if (key == "show_menu") config.show_menu = (value == "1" || value == "true");
        else if (key == "bEsp") config.bEsp = (value == "1" || value == "true");
        else if (key == "bEspTeamCheck") config.bEspTeamCheck = (value == "1" || value == "true");
        else if (key == "bEspBox") config.bEspBox = (value == "1" || value == "true");
        else if (key == "boxType") config.boxType = std::stoi(value);
        else if (key == "boxThickness") config.boxThickness = std::stof(value);
        else if (key == "bBoxFill") config.bBoxFill = (value == "1" || value == "true");
        else if (key == "espBoxColor") StringToFloatArray(value, config.espBoxColor);
        else if (key == "espFillColor") StringToFloatArray(value, config.espFillColor);
        else if (key == "bEspSkeleton") config.bEspSkeleton = (value == "1" || value == "true");
        else if (key == "espSkeletonColor") StringToFloatArray(value, config.espSkeletonColor);
        else if (key == "bEspName") config.bEspName = (value == "1" || value == "true");
        else if (key == "espNameColor") StringToFloatArray(value, config.espNameColor);
        else if (key == "bEspHealthBar") config.bEspHealthBar = (value == "1" || value == "true");
        else if (key == "espHealthColor") StringToFloatArray(value, config.espHealthColor);
        else if (key == "bEspArmorBar") config.bEspArmorBar = (value == "1" || value == "true");
        else if (key == "espArmorColor") StringToFloatArray(value, config.espArmorColor);
        else if (key == "bEspHeadCircle") config.bEspHeadCircle = (value == "1" || value == "true");
        else if (key == "espHeadCircleColor") StringToFloatArray(value, config.espHeadCircleColor);
        else if (key == "bEspDistance") config.bEspDistance = (value == "1" || value == "true");
        else if (key == "espDistanceColor") StringToFloatArray(value, config.espDistanceColor);
        else if (key == "bEspSnapline") config.bEspSnapline = (value == "1" || value == "true");
        else if (key == "espSnaplineColor") StringToFloatArray(value, config.espSnaplineColor);
        else if (key == "bEspViewDirection") config.bEspViewDirection = (value == "1" || value == "true");
        else if (key == "espViewDirectionColor") StringToFloatArray(value, config.espViewDirectionColor);
        else if (key == "espViewDirectionLength") config.espViewDirectionLength = std::stof(value);
        else if (key == "bHeadShotLine") config.bHeadShotLine = (value == "1" || value == "true");
        else if (key == "headShotColor") StringToFloatArray(value, config.headShotColor);
        // Glow
        else if (key == "bGlow") config.bGlow = (value == "1" || value == "true");
        else if (key == "glowType") config.glowType = std::stoi(value);
        else if (key == "bGlowTeamCheck") config.bGlowTeamCheck = (value == "1" || value == "true");
        else if (key == "glowColor") StringToFloatArray(value, config.glowColor);
        else if (key == "glowTeamColor") StringToFloatArray(value, config.glowTeamColor);
        else if (key == "glowEnemyColor") StringToFloatArray(value, config.glowEnemyColor);
        // Misc
        else if (key == "bBhop") config.bBhop = (value == "1" || value == "true");
        else if (key == "bNoFlash") config.bNoFlash = (value == "1" || value == "true");
        else if (key == "menuKey") config.menuKey = std::stoi(value);
        else if (key == "bStreamproof") config.bStreamproof = (value == "1" || value == "true");
        // Aimbot
        else if (key == "bAimbot") config.bAimbot = (value == "1" || value == "true");
        else if (key == "aimMode") config.aimMode = std::stoi(value);
        else if (key == "aimKey") config.aimKey = std::stoi(value);
        else if (key == "hitboxMask") config.hitboxMask = std::stoi(value);
        else if (key == "aimFov") config.aimFov = std::stof(value);
        else if (key == "bDrawFov") config.bDrawFov = (value == "1" || value == "true");
        else if (key == "aimSmooth") config.aimSmooth = std::stof(value);
        else if (key == "bAimSmoothRandom") config.bAimSmoothRandom = (value == "1" || value == "true");
        else if (key == "aimSmoothMin") config.aimSmoothMin = std::stof(value);
        else if (key == "aimSmoothMax") config.aimSmoothMax = std::stof(value);
        else if (key == "bAimJitter") config.bAimJitter = (value == "1" || value == "true");
        else if (key == "aimJitterAmount") config.aimJitterAmount = std::stof(value);
        else if (key == "bVisibleCheck") config.bVisibleCheck = (value == "1" || value == "true");
        else if (key == "aimFovColor") StringToFloatArray(value, config.aimFovColor);
        else if (key == "bEspWeapon") config.bEspWeapon = (value == "1" || value == "true");
        else if (key == "espWeaponColor") StringToFloatArray(value, config.espWeaponColor);
        // Triggerbot
        else if (key == "bTriggerbot") config.bTriggerbot = (value == "1" || value == "true");
        else if (key == "triggerMode") config.triggerMode = std::stoi(value);
        else if (key == "triggerKey") config.triggerKey = std::stoi(value);
        else if (key == "triggerReactionTime") config.triggerReactionTime = std::stoi(value);
        else if (key == "triggerShotCooldown") config.triggerShotCooldown = std::stoi(value);
        else if (key == "bTriggerReactionRandom") config.bTriggerReactionRandom = (value == "1" || value == "true");   // NEW
        else if (key == "triggerReactionMin") config.triggerReactionMin = std::stoi(value);                             // NEW
        else if (key == "triggerReactionMax") config.triggerReactionMax = std::stoi(value);                             // NEW
        else if (key == "bTriggerCooldownRandom") config.bTriggerCooldownRandom = (value == "1" || value == "true");   // NEW
        else if (key == "triggerCooldownMin") config.triggerCooldownMin = std::stoi(value);                             // NEW
        else if (key == "triggerCooldownMax") config.triggerCooldownMax = std::stoi(value);                             // NEW
        else if (key == "bFovChanger") config.bFovChanger = (value == "1" || value == "true");
        else if (key == "fovValue") config.fovValue = std::stof(value);
        else if (key == "bRadar") config.bRadar = (value == "1" || value == "true");
        else if (key == "bBhop") config.bBhop = (value == "1" || value == "true");
        else if (key == "bNoFlash") config.bNoFlash = (value == "1" || value == "true");
        else if (key == "menuKey") config.menuKey = std::stoi(value);
        else if (key == "bStreamproof") config.bStreamproof = (value == "1" || value == "true");
        else if (key == "bBombTimer") config.bBombTimer = (value == "1" || value == "true");   // ADD
    }
    file.close();
    return true;
}

bool Config::SaveConfig(const std::string& filename) {
    std::string path = "C:\\Velocity\\configs\\" + filename;
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "show_menu = " << (config.show_menu ? 1 : 0) << "\n";
    file << "bEsp = " << (config.bEsp ? 1 : 0) << "\n";
    file << "bEspTeamCheck = " << (config.bEspTeamCheck ? 1 : 0) << "\n";
    file << "bEspBox = " << (config.bEspBox ? 1 : 0) << "\n";
    file << "boxType = " << config.boxType << "\n";
    file << "boxThickness = " << config.boxThickness << "\n";
    file << "bBoxFill = " << (config.bBoxFill ? 1 : 0) << "\n";
    file << "espBoxColor = " << FloatArrayToString(config.espBoxColor) << "\n";
    file << "espFillColor = " << FloatArrayToString(config.espFillColor) << "\n";
    file << "bEspSkeleton = " << (config.bEspSkeleton ? 1 : 0) << "\n";
    file << "espSkeletonColor = " << FloatArrayToString(config.espSkeletonColor) << "\n";
    file << "bEspName = " << (config.bEspName ? 1 : 0) << "\n";
    file << "espNameColor = " << FloatArrayToString(config.espNameColor) << "\n";
    file << "bEspHealthBar = " << (config.bEspHealthBar ? 1 : 0) << "\n";
    file << "espHealthColor = " << FloatArrayToString(config.espHealthColor) << "\n";
    file << "bEspArmorBar = " << (config.bEspArmorBar ? 1 : 0) << "\n";
    file << "espArmorColor = " << FloatArrayToString(config.espArmorColor) << "\n";
    file << "bEspHeadCircle = " << (config.bEspHeadCircle ? 1 : 0) << "\n";
    file << "espHeadCircleColor = " << FloatArrayToString(config.espHeadCircleColor) << "\n";
    file << "bEspDistance = " << (config.bEspDistance ? 1 : 0) << "\n";
    file << "espDistanceColor = " << FloatArrayToString(config.espDistanceColor) << "\n";
    file << "bEspSnapline = " << (config.bEspSnapline ? 1 : 0) << "\n";
    file << "espSnaplineColor = " << FloatArrayToString(config.espSnaplineColor) << "\n";
    file << "bEspViewDirection = " << (config.bEspViewDirection ? 1 : 0) << "\n";
    file << "espViewDirectionColor = " << FloatArrayToString(config.espViewDirectionColor) << "\n";
    file << "espViewDirectionLength = " << config.espViewDirectionLength << "\n";
    file << "bHeadShotLine = " << (config.bHeadShotLine ? 1 : 0) << "\n";
    file << "headShotColor = " << FloatArrayToString(config.headShotColor) << "\n";
    // Glow
    file << "bGlow = " << (config.bGlow ? 1 : 0) << "\n";
    file << "glowType = " << config.glowType << "\n";
    file << "bGlowTeamCheck = " << (config.bGlowTeamCheck ? 1 : 0) << "\n";
    file << "glowColor = " << FloatArrayToString(config.glowColor) << "\n";
    file << "glowTeamColor = " << FloatArrayToString(config.glowTeamColor) << "\n";
    file << "glowEnemyColor = " << FloatArrayToString(config.glowEnemyColor) << "\n";
    // Misc
    file << "bBhop = " << (config.bBhop ? 1 : 0) << "\n";
    file << "bNoFlash = " << (config.bNoFlash ? 1 : 0) << "\n";
    file << "menuKey = " << config.menuKey << "\n";
    file << "bStreamproof = " << (config.bStreamproof ? 1 : 0) << "\n";
    file << "bBhop = " << (config.bBhop ? 1 : 0) << "\n";
    file << "bNoFlash = " << (config.bNoFlash ? 1 : 0) << "\n";
    file << "menuKey = " << config.menuKey << "\n";
    file << "bStreamproof = " << (config.bStreamproof ? 1 : 0) << "\n";
    file << "bBombTimer = " << (config.bBombTimer ? 1 : 0) << "\n";   // ADD
    // Aimbot
    file << "bAimbot = " << (config.bAimbot ? 1 : 0) << "\n";
    file << "aimMode = " << config.aimMode << "\n";
    file << "aimKey = " << config.aimKey << "\n";
    file << "hitboxMask = " << config.hitboxMask << "\n";
    file << "aimFov = " << config.aimFov << "\n";
    file << "bDrawFov = " << (config.bDrawFov ? 1 : 0) << "\n";
    file << "aimSmooth = " << config.aimSmooth << "\n";
    file << "bAimSmoothRandom = " << (config.bAimSmoothRandom ? 1 : 0) << "\n";
    file << "aimSmoothMin = " << config.aimSmoothMin << "\n";
    file << "aimSmoothMax = " << config.aimSmoothMax << "\n";
    file << "bAimJitter = " << (config.bAimJitter ? 1 : 0) << "\n";
    file << "aimJitterAmount = " << config.aimJitterAmount << "\n";
    file << "bVisibleCheck = " << (config.bVisibleCheck ? 1 : 0) << "\n";
    file << "aimFovColor = " << FloatArrayToString(config.aimFovColor) << "\n";
    file << "bEspWeapon = " << (config.bEspWeapon ? 1 : 0) << "\n";
    file << "espWeaponColor = " << FloatArrayToString(config.espWeaponColor) << "\n";
    // Triggerbot
    file << "bTriggerbot = " << (config.bTriggerbot ? 1 : 0) << "\n";
    file << "triggerMode = " << config.triggerMode << "\n";
    file << "triggerKey = " << config.triggerKey << "\n";
    file << "triggerReactionTime = " << config.triggerReactionTime << "\n";
    file << "triggerShotCooldown = " << config.triggerShotCooldown << "\n";
    file << "bTriggerReactionRandom = " << (config.bTriggerReactionRandom ? 1 : 0) << "\n";   // NEW
    file << "triggerReactionMin = " << config.triggerReactionMin << "\n";                     // NEW
    file << "triggerReactionMax = " << config.triggerReactionMax << "\n";                     // NEW
    file << "bTriggerCooldownRandom = " << (config.bTriggerCooldownRandom ? 1 : 0) << "\n";   // NEW
    file << "triggerCooldownMin = " << config.triggerCooldownMin << "\n";                     // NEW
    file << "triggerCooldownMax = " << config.triggerCooldownMax << "\n";                     // NEW
    file << "bFovChanger = " << (config.bFovChanger ? 1 : 0) << "\n";
    file << "fovValue = " << config.fovValue << "\n";
    file << "bRadar = " << (config.bRadar ? 1 : 0) << "\n";
    file.close();
    return true;
}

bool Config::CreateConfig(const std::string& filename) {
    std::string name = filename;
    if (name.find(".ini") == std::string::npos) name += ".ini";
    std::string path = "C:\\Velocity\\configs\\" + name;
    if (fs::exists(path)) return false;

    Config defaultCfg;
    defaultCfg.SetDefaults();

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "show_menu = " << (defaultCfg.show_menu ? 1 : 0) << "\n";
    file << "bEsp = " << (defaultCfg.bEsp ? 1 : 0) << "\n";
    file << "bEspTeamCheck = " << (defaultCfg.bEspTeamCheck ? 1 : 0) << "\n";
    file << "bEspBox = " << (defaultCfg.bEspBox ? 1 : 0) << "\n";
    file << "boxType = " << defaultCfg.boxType << "\n";
    file << "boxThickness = " << defaultCfg.boxThickness << "\n";
    file << "bBoxFill = " << (defaultCfg.bBoxFill ? 1 : 0) << "\n";
    file << "espBoxColor = " << FloatArrayToString(defaultCfg.espBoxColor) << "\n";
    file << "espFillColor = " << FloatArrayToString(defaultCfg.espFillColor) << "\n";
    file << "bEspSkeleton = " << (defaultCfg.bEspSkeleton ? 1 : 0) << "\n";
    file << "espSkeletonColor = " << FloatArrayToString(defaultCfg.espSkeletonColor) << "\n";
    file << "bEspName = " << (defaultCfg.bEspName ? 1 : 0) << "\n";
    file << "espNameColor = " << FloatArrayToString(defaultCfg.espNameColor) << "\n";
    file << "bEspHealthBar = " << (defaultCfg.bEspHealthBar ? 1 : 0) << "\n";
    file << "espHealthColor = " << FloatArrayToString(defaultCfg.espHealthColor) << "\n";
    file << "bEspArmorBar = " << (defaultCfg.bEspArmorBar ? 1 : 0) << "\n";
    file << "espArmorColor = " << FloatArrayToString(defaultCfg.espArmorColor) << "\n";
    file << "bEspHeadCircle = " << (defaultCfg.bEspHeadCircle ? 1 : 0) << "\n";
    file << "espHeadCircleColor = " << FloatArrayToString(defaultCfg.espHeadCircleColor) << "\n";
    file << "bEspDistance = " << (defaultCfg.bEspDistance ? 1 : 0) << "\n";
    file << "espDistanceColor = " << FloatArrayToString(defaultCfg.espDistanceColor) << "\n";
    file << "bEspSnapline = " << (defaultCfg.bEspSnapline ? 1 : 0) << "\n";
    file << "espSnaplineColor = " << FloatArrayToString(defaultCfg.espSnaplineColor) << "\n";
    file << "bEspViewDirection = " << (defaultCfg.bEspViewDirection ? 1 : 0) << "\n";
    file << "espViewDirectionColor = " << FloatArrayToString(defaultCfg.espViewDirectionColor) << "\n";
    file << "espViewDirectionLength = " << defaultCfg.espViewDirectionLength << "\n";
    file << "bHeadShotLine = " << (defaultCfg.bHeadShotLine ? 1 : 0) << "\n";
    file << "headShotColor = " << FloatArrayToString(defaultCfg.headShotColor) << "\n";
    // Glow
    file << "bGlow = " << (defaultCfg.bGlow ? 1 : 0) << "\n";
    file << "glowType = " << defaultCfg.glowType << "\n";
    file << "bGlowTeamCheck = " << (defaultCfg.bGlowTeamCheck ? 1 : 0) << "\n";
    file << "glowColor = " << FloatArrayToString(defaultCfg.glowColor) << "\n";
    file << "glowTeamColor = " << FloatArrayToString(defaultCfg.glowTeamColor) << "\n";
    file << "glowEnemyColor = " << FloatArrayToString(defaultCfg.glowEnemyColor) << "\n";
    // Misc
    file << "bBhop = " << (config.bBhop ? 1 : 0) << "\n";
    file << "bNoFlash = " << (config.bNoFlash ? 1 : 0) << "\n";
    file << "menuKey = " << config.menuKey << "\n";
    file << "bStreamproof = " << (config.bStreamproof ? 1 : 0) << "\n";
    file << "bBombTimer = " << (config.bBombTimer ? 1 : 0) << "\n";   // ADD
    // Aimbot
    file << "bAimbot = " << (defaultCfg.bAimbot ? 1 : 0) << "\n";
    file << "aimMode = " << defaultCfg.aimMode << "\n";
    file << "aimKey = " << defaultCfg.aimKey << "\n";
    file << "hitboxMask = " << defaultCfg.hitboxMask << "\n";
    file << "aimFov = " << defaultCfg.aimFov << "\n";
    file << "bDrawFov = " << (defaultCfg.bDrawFov ? 1 : 0) << "\n";
    file << "aimSmooth = " << defaultCfg.aimSmooth << "\n";
    file << "bAimSmoothRandom = " << (defaultCfg.bAimSmoothRandom ? 1 : 0) << "\n";
    file << "aimSmoothMin = " << defaultCfg.aimSmoothMin << "\n";
    file << "aimSmoothMax = " << defaultCfg.aimSmoothMax << "\n";
    file << "bAimJitter = " << (defaultCfg.bAimJitter ? 1 : 0) << "\n";
    file << "aimJitterAmount = " << defaultCfg.aimJitterAmount << "\n";
    file << "bVisibleCheck = " << (defaultCfg.bVisibleCheck ? 1 : 0) << "\n";
    file << "aimFovColor = " << FloatArrayToString(defaultCfg.aimFovColor) << "\n";
    file << "bEspWeapon = " << (defaultCfg.bEspWeapon ? 1 : 0) << "\n";
    file << "espWeaponColor = " << FloatArrayToString(defaultCfg.espWeaponColor) << "\n";
    file << "bStreamproof = " << (config.bStreamproof ? 1 : 0) << "\n";
    // Triggerbot
    file << "bTriggerbot = " << (defaultCfg.bTriggerbot ? 1 : 0) << "\n";
    file << "triggerMode = " << defaultCfg.triggerMode << "\n";
    file << "triggerKey = " << defaultCfg.triggerKey << "\n";
    file << "triggerReactionTime = " << defaultCfg.triggerReactionTime << "\n";
    file << "triggerShotCooldown = " << defaultCfg.triggerShotCooldown << "\n";
    file << "bTriggerReactionRandom = " << (defaultCfg.bTriggerReactionRandom ? 1 : 0) << "\n";   // NEW
    file << "triggerReactionMin = " << defaultCfg.triggerReactionMin << "\n";                     // NEW
    file << "triggerReactionMax = " << defaultCfg.triggerReactionMax << "\n";                     // NEW
    file << "bTriggerCooldownRandom = " << (defaultCfg.bTriggerCooldownRandom ? 1 : 0) << "\n";   // NEW
    file << "triggerCooldownMin = " << defaultCfg.triggerCooldownMin << "\n";                     // NEW
    file << "triggerCooldownMax = " << defaultCfg.triggerCooldownMax << "\n";                     // NEW
    file << "bFovChanger = " << (config.bFovChanger ? 1 : 0) << "\n";
    file << "fovValue = " << config.fovValue << "\n";
    file << "bRadar = " << (config.bRadar ? 1 : 0) << "\n";

    file.close();
    return true;
}

bool Config::DeleteConfig(const std::string& filename) {
    std::string path = "C:\\Velocity\\configs\\" + filename;
    if (!fs::exists(path)) return false;
    return fs::remove(path);
}

bool Config::RenameConfig(const std::string& oldName, const std::string& newName) {
    std::string oldPath = "C:\\Velocity\\configs\\" + oldName;
    std::string newPath = "C:\\Velocity\\configs\\" + newName;
    if (!fs::exists(oldPath)) return false;
    if (fs::exists(newPath)) return false;
    fs::rename(oldPath, newPath);
    return true;
}