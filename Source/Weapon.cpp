#include "Weapon.h"
#include "Memory.h"
#include "Offsets.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

extern Memory mem;

// ------------------------------------------------------------
// Map raw VData names (without "weapon_") to clean display names
// ------------------------------------------------------------
static const char* GetDisplayNameFromRaw(const char* raw) {
    if (!raw) return nullptr;

    std::string name = raw;
    // Convert to lowercase for comparison
    for (char& c : name) c = tolower(c);

    // Pistols
    if (name == "usp_silencer") return "USP-S";
    if (name == "usp")           return "USP";
    if (name == "glock")         return "Glock-18";
    if (name == "deagle")        return "Deagle";
    if (name == "p250")          return "P250";
    if (name == "fiveseven")     return "Five-Seven";
    if (name == "dualies")       return "Dual Berettas";
    if (name == "tec9")          return "Tec-9";
    if (name == "cz75a")         return "CZ75-Auto";
    if (name == "r8_revolver")   return "R8 Revolver";
    if (name == "hkp2000")       return "P2000";

    // Rifles
    if (name == "ak47")          return "AK-47";
    if (name == "m4a4")          return "M4A4";
    if (name == "m4a4_silencer") return "M4A1-S";
    if (name == "m4a1_silencer") return "M4A1-S";  // alternative
    if (name == "aug")           return "AUG";
    if (name == "famas")         return "FAMAS";
    if (name == "galil")         return "Galil AR";
    if (name == "sg556")         return "SG 553";

    // Snipers
    if (name == "awp")           return "AWP";
    if (name == "ssg08")         return "SSG 08";
    if (name == "g3sg1")         return "G3SG1";
    if (name == "scar20")        return "SCAR-20";

    // SMGs
    if (name == "mac10")         return "MAC-10";
    if (name == "mp5sd")         return "MP5-SD";
    if (name == "mp7")           return "MP7";
    if (name == "mp9")           return "MP9";
    if (name == "bizon")         return "PP-Bizon";
    if (name == "p90")           return "P90";
    if (name == "ump45")         return "UMP-45";

    // Shotguns
    if (name == "nova")          return "Nova";
    if (name == "sawedoff")      return "Sawed-Off";
    if (name == "xm1014")        return "XM1014";
    if (name == "mag7")          return "MAG-7";

    // LMGs
    if (name == "m249")          return "M249";
    if (name == "negev")         return "Negev";

    // Grenades
    if (name == "flashbang")     return "Flashbang";
    if (name == "hegrenade")     return "HE Grenade";
    if (name == "smokegrenade")  return "Smoke Grenade";
    if (name == "molotov")       return "Molotov";
    if (name == "decoy")         return "Decoy";
    if (name == "incendiary")    return "Incendiary";

    // Other
    if (name == "zeus")          return "Zeus x27";
    if (name == "c4")            return "C4";
    if (name == "defuser")       return "Defuser";
    if (name == "healthshot")    return "Healthshot";

    // Knife: any name containing "knife"
    if (name.find("knife") != std::string::npos)
        return "Knife";

    // If not found, return nullptr – caller will use raw name as fallback
    return nullptr;
}

// ------------------------------------------------------------
// Weapon ID -> display name mapping (from item definition index)
// ------------------------------------------------------------
static const char* GetWeaponNameById(int id) {
    // Knives: generic "Knife" for any knife ID (500–512 + 42)
    if ((id >= 500 && id <= 512) || id == 42)
        return "Knife";

    switch (id) {
        // Pistols
    case 1:  return "Deagle";
    case 2:  return "Dual Berettas";
    case 3:  return "Five-Seven";
    case 4:  return "Glock-18";
    case 32: return "P2000";
    case 61: return "USP-S";
    case 36: return "P250";
    case 63: return "CZ75-Auto";
    case 30: return "Tec-9";
    case 64: return "R8 Revolver";
        // Rifles
    case 7:  return "AK-47";
    case 16: return "M4A4";
    case 60: return "M4A1-S";
    case 8:  return "AUG";
    case 10: return "FAMAS";
    case 13: return "Galil AR";
    case 39: return "SG 553";
        // Snipers
    case 9:  return "AWP";
    case 40: return "SSG 08";
    case 11: return "G3SG1";
    case 38: return "SCAR-20";
        // SMGs
    case 17: return "MAC-10";
    case 23: return "MP5-SD";
    case 33: return "MP7";
    case 34: return "MP9";
    case 26: return "PP-Bizon";
    case 19: return "P90";
    case 24: return "UMP-45";
        // Shotguns
    case 35: return "Nova";
    case 29: return "Sawed-Off";
    case 25: return "XM1014";
    case 27: return "MAG-7";
        // LMGs
    case 14: return "M249";
    case 28: return "Negev";
        // Grenades
    case 43: return "Flashbang";
    case 44: return "HE Grenade";
    case 45: return "Smoke Grenade";
    case 46: return "Molotov";
    case 47: return "Decoy";
    case 48: return "Incendiary";
        // Other
    case 31: return "Zeus x27";
    case 49: return "C4";
    case 55: return "Defuser";
    case 57: return "Healthshot";
    default: return nullptr;
    }
}

// ------------------------------------------------------------
// Try multiple ways to get m_iItemDefinitionIndex
// ------------------------------------------------------------
static int GetWeaponDefinitionIndex(uintptr_t weapon) {
    // Method 1: attribute‑manager path (standard)
    uintptr_t attrManager = mem.Read<uintptr_t>(weapon + offsets::entity::m_AttributeManager);
    if (attrManager) {
        uintptr_t econItem = mem.Read<uintptr_t>(attrManager + offsets::attributeManager::m_Item);
        if (econItem) {
            int id = mem.Read<int>(econItem + offsets::econItemView::m_iItemDefinitionIndex);
            if (id > 0) return id;
        }
    }

    // Method 2: direct read from weapon entity (fallback)
    int id = mem.Read<int>(weapon + offsets::weaponEntity::m_iItemDefinitionIndex);
    if (id > 0) return id;

    return 0;
}

// ------------------------------------------------------------
// Get weapon name from VData (fallback)
// ------------------------------------------------------------
static bool GetWeaponNameFromVData(uintptr_t weapon, char* outName, size_t outSize) {
    uintptr_t vdata = mem.Read<uintptr_t>(weapon + offsets::m_nSubclassID + 0x8);
    if (!vdata) return false;

    uintptr_t namePtr = mem.Read<uintptr_t>(vdata + offsets::m_szName);
    if (!namePtr) return false;

    char rawName[64] = {};
    if (!mem.ReadRaw(namePtr, rawName, sizeof(rawName) - 1)) return false;

    // Remove "weapon_" prefix
    const char* prefix = "weapon_";
    if (strncmp(rawName, prefix, strlen(prefix)) == 0) {
        strncpy_s(outName, outSize, rawName + strlen(prefix), _TRUNCATE);
        return true;
    }
    strncpy_s(outName, outSize, rawName, _TRUNCATE);
    return true;
}

// ------------------------------------------------------------
// Main exported function
// ------------------------------------------------------------
const char* GetWeaponNameFromPawn(uintptr_t pawn, char* outName, size_t outSize) {
    if (!pawn || !outName || outSize == 0) return nullptr;

    // 1) Get weapon services
    uintptr_t weaponServices = mem.Read<uintptr_t>(pawn + offsets::playerPawn::m_pWeaponServices);
    if (!weaponServices) {
        snprintf(outName, outSize, "ERR:noWSvc");
        return outName;
    }

    // 2) Active weapon handle
    uint32_t activeWeaponHandle = mem.Read<uint32_t>(weaponServices + offsets::weaponServices::m_hActiveWeapon);
    if (!activeWeaponHandle || activeWeaponHandle == 0xFFFFFFFF) {
        snprintf(outName, outSize, "ERR:noWpnHdl");
        return outName;
    }

    // 3) Resolve handle to weapon entity
    uintptr_t entityList = mem.Read<uintptr_t>(mem.client + offsets::client::dwEntityList);
    if (!entityList) {
        snprintf(outName, outSize, "ERR:noEntList");
        return outName;
    }

    uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * ((activeWeaponHandle & 0x7FFF) >> 9) + 16));
    if (!listEntry) {
        snprintf(outName, outSize, "ERR:noListEntry");
        return outName;
    }

    uintptr_t weapon = mem.Read<uintptr_t>(listEntry + 112 * ((activeWeaponHandle & 0x7FFF) & 0x1FF));
    if (!weapon) {
        snprintf(outName, outSize, "ERR:noWeapon");
        return outName;
    }

    // 4) Try ID mapping (using the improved GetWeaponDefinitionIndex)
    int defIndex = GetWeaponDefinitionIndex(weapon);
    if (defIndex > 0) {
        const char* name = GetWeaponNameById(defIndex);
        if (name) {
            strncpy_s(outName, outSize, name, _TRUNCATE);
            return outName;
        }
    }

    // 5) Fallback: get raw VData name and apply override mapping
    char rawName[64] = {};
    if (GetWeaponNameFromVData(weapon, rawName, sizeof(rawName))) {
        const char* mapped = GetDisplayNameFromRaw(rawName);
        if (mapped) {
            strncpy_s(outName, outSize, mapped, _TRUNCATE);
            return outName;
        }
        // Otherwise, use raw name but with first letter capitalized
        // (simple fallback to avoid all-lowercase)
        if (rawName[0] >= 'a' && rawName[0] <= 'z')
            rawName[0] -= 32;
        strncpy_s(outName, outSize, rawName, _TRUNCATE);
        return outName;
    }

    // 6) Last resort: show subclass ID
    uint16_t subclassID = mem.Read<uint16_t>(weapon + offsets::m_nSubclassID);
    snprintf(outName, outSize, "SUB:0x%04X", subclassID);
    return outName;
}