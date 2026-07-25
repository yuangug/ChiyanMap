#pragma once
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <windows.h>
#include <ll/api/memory/Hook.h>
#include <mc/client/game/ClientInstance.h>
#include <mc/client/player/LocalPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/deps/core/math/Color.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/biome/Biome.h>
#include <mc/world/level/material/Material.h>
#include <mc/deps/shared_types/v1_26_20/block/MaterialType.h>
#include "state/WaypointManager.h"
#include "state/DeathPointManager.h"
#include <mc/world/level/BlockPos.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/level/Level.h>
#include <mc/world/actor/Actor.h>
#include <mc/legacy/ActorRuntimeID.h>
#include <mc/world/actor/ActorCategory.h>
#include <mc/world/gamemode/GameMode.h>
#include <mc/world/gamemode/InteractionResult.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/ActorEvent.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/item/ItemStack.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/world/phys/HitResult.h>
#include <mc/world/actor/player/SerializedSkinRef.h>
#include <mc/world/actor/player/SerializedSkinImpl.h>
#include <mc/world/actor/player/SkinImage.h>
#include <mc/deps/core/image/Image.h>
#include <mc/platform/UUID.h>
#include "state/MapRenderState.h"
#include "state/MapCacheManager.h"
#include "state/LanguageManager.h"

using BRD_RegisterClientInstanceUpdateCallback_t = void(*)(void(__stdcall*)(void*, unsigned char));
using BRD_UnregisterClientInstanceUpdateCallback_t = void(*)(void(__stdcall*)(void*, unsigned char));
using BRD_GetClientInstance_t = void*(*)();

inline BRD_RegisterClientInstanceUpdateCallback_t g_brdRegisterClientInstanceUpdateCallback = nullptr;
inline BRD_UnregisterClientInstanceUpdateCallback_t g_brdUnregisterClientInstanceUpdateCallback = nullptr;
inline BRD_GetClientInstance_t g_brdGetClientInstance = nullptr;
inline bool g_registeredBRDClientInstanceCallback = false;

inline void HandleClientInstanceUpdate(ClientInstance* clientInstance, bool isInitFinished);
inline void __stdcall OnBRDClientInstanceUpdate(void* clientInstance, unsigned char isInitFinished) {
    HandleClientInstanceUpdate(static_cast<ClientInstance*>(clientInstance), isInitFinished != 0);
}

inline bool ResolveBRDClientInstanceApi() {
    HMODULE brd = GetModuleHandleA("BetterRenderDragon.dll");
    if (!brd) return false;
    g_brdRegisterClientInstanceUpdateCallback = reinterpret_cast<BRD_RegisterClientInstanceUpdateCallback_t>(
        GetProcAddress(brd, "BRD_RegisterClientInstanceUpdateCallback")
    );
    g_brdUnregisterClientInstanceUpdateCallback = reinterpret_cast<BRD_UnregisterClientInstanceUpdateCallback_t>(
        GetProcAddress(brd, "BRD_UnregisterClientInstanceUpdateCallback")
    );
    g_brdGetClientInstance = reinterpret_cast<BRD_GetClientInstance_t>(GetProcAddress(brd, "BRD_GetClientInstance"));
    return g_brdRegisterClientInstanceUpdateCallback && g_brdUnregisterClientInstanceUpdateCallback;
}

inline bool RegisterBRDClientInstanceUpdateCallback() {
    if (g_registeredBRDClientInstanceCallback) return true;
    if (!ResolveBRDClientInstanceApi()) return false;
    g_brdRegisterClientInstanceUpdateCallback(OnBRDClientInstanceUpdate);
    g_registeredBRDClientInstanceCallback = true;
    if (g_brdGetClientInstance) {
        if (void* clientInstance = g_brdGetClientInstance()) {
            OnBRDClientInstanceUpdate(clientInstance, 1);
        }
    }
    return true;
}

inline void UnregisterBRDClientInstanceUpdateCallback() {
    if (!g_registeredBRDClientInstanceCallback) return;
    if (g_brdUnregisterClientInstanceUpdateCallback) {
        g_brdUnregisterClientInstanceUpdateCallback(OnBRDClientInstanceUpdate);
    }
    g_registeredBRDClientInstanceCallback = false;
}

inline uint64_t HashPlayerSkinHead(const uint8_t* pixels, size_t size) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= pixels[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

inline void InvalidatePlayerSkinHead(const std::string& uuid) {
    if (uuid.empty()) return;
    std::lock_guard<std::mutex> lock(g_playerSkinMutex);
    auto it = g_playerSkinHeads.find(uuid);
    if (it != g_playerSkinHeads.end() && it->second.valid) {
        it->second.valid = false;
        ++it->second.revision;
    }
}

inline bool IsLocalRadarPlayer(ClientInstance const& clientInstance, LocalPlayer const& localPlayer, Actor const& actor) {
    if (&actor == static_cast<Actor const*>(&localPlayer)) return true;
    if (!actor.isPlayer()) return false;

    if (actor.hasRuntimeID() && localPlayer.hasRuntimeID()
        && actor.getRuntimeID() == localPlayer.getRuntimeID()) {
        return true;
    }

    const auto& actorUniqueId = actor.getOrCreateUniqueID();
    if (actorUniqueId == localPlayer.getOrCreateUniqueID()
        || clientInstance.isLocalSplitscreenWith(actorUniqueId)) {
        return true;
    }

    auto const& candidatePlayer = static_cast<Player const&>(actor);
    if (candidatePlayer.getUuid() == localPlayer.getUuid()) return true;

    // Some servers mirror the local client as a separate network player entity.
    // That proxy has different actor IDs and UUID, but retains the player's name tag.
    auto const& localNameTag = localPlayer.getNameTag();
    return !localNameTag.empty() && candidatePlayer.getNameTag() == localNameTag;
}

// 提取并合成玩家皮肤的 8x8 头部正面与第二层。
inline void ExtractPlayerSkinHead(class Player* player, const std::string& uuid) {
    if (!player || uuid.empty()) return;
    try {
        if (!player->mSkin) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& skinRef = *player->mSkin;
        if (!skinRef.mSkinImpl) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& threadOwner = *skinRef.mSkinImpl;
        auto& skinImpl = threadOwner.mObject;
        if (skinImpl.mIsPersona) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& skinImage = skinImpl.mSkinImage.get();
        auto& img = static_cast<mce::Image&>(skinImage);
        if (img.imageFormat != mce::ImageFormat::RGBA8Unorm || img.mWidth < 8 || img.mHeight < 16 || img.mWidth % 8 != 0) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        const uint8_t* pixels = img.mImageBytes.data();
        if (!pixels) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }

        const int unit = static_cast<int>(img.mWidth) / 8;
        const int baseHeadX = unit;
        const int baseHeadY = unit;
        const int outerHeadX = unit * 5;
        if (unit < 8 || baseHeadY + unit > static_cast<int>(img.mHeight) || outerHeadX + unit > static_cast<int>(img.mWidth)) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }

        PlayerSkinHead head;
        for (int y = 0; y < 8; ++y) {
            const int sourceY0 = baseHeadY + (y * unit) / 8;
            const int sourceY1 = baseHeadY + ((y + 1) * unit) / 8;
            for (int x = 0; x < 8; ++x) {
                const int sourceX0 = baseHeadX + (x * unit) / 8;
                const int sourceX1 = baseHeadX + ((x + 1) * unit) / 8;
                uint32_t totals[4]{};
                uint32_t sampleCount = 0;
                for (int sourceY = sourceY0; sourceY < sourceY1; ++sourceY) {
                    for (int sourceX = sourceX0; sourceX < sourceX1; ++sourceX) {
                        const uint8_t* base = pixels + (sourceY * static_cast<int>(img.mWidth) + sourceX) * 4;
                        const uint8_t* outer = pixels + (sourceY * static_cast<int>(img.mWidth) + outerHeadX + (sourceX - baseHeadX)) * 4;
                        const uint32_t outerAlpha = outer[3];
                        const uint32_t inverseOuterAlpha = 255 - outerAlpha;
                        const uint32_t baseAlpha = base[3];
                        const uint32_t outputAlpha = outerAlpha + (baseAlpha * inverseOuterAlpha + 127) / 255;
                        for (int channel = 0; channel < 3; ++channel) {
                            const uint32_t premultiplied = outer[channel] * outerAlpha
                                + (base[channel] * baseAlpha * inverseOuterAlpha + 127) / 255;
                            totals[channel] += outputAlpha == 0
                                ? 0
                                : (premultiplied + outputAlpha / 2) / outputAlpha;
                        }
                        totals[3] += outputAlpha;
                        ++sampleCount;
                    }
                }
                const int destination = (y * 8 + x) * 4;
                for (int channel = 0; channel < 4; ++channel) {
                    head.pixels[destination + channel] = static_cast<uint8_t>((totals[channel] + sampleCount / 2) / sampleCount);
                }
            }
        }
        head.fingerprint = HashPlayerSkinHead(head.pixels, sizeof(head.pixels));
        head.valid = true;
        std::lock_guard<std::mutex> lock(g_playerSkinMutex);
        auto& cachedHead = g_playerSkinHeads[uuid];
        if (cachedHead.valid && cachedHead.fingerprint == head.fingerprint
            && std::memcmp(cachedHead.pixels, head.pixels, sizeof(head.pixels)) == 0) {
            return;
        }
        head.revision = cachedHead.revision + 1;
        g_playerSkinHeads[uuid] = head;
    } catch (...) {}
}

extern float g_playerX;
extern float g_playerY;
extern float g_playerZ;
extern float g_playerYaw;
extern bool  g_hasPlayer;
extern LocalPlayer* g_localPlayer;
extern std::string g_localPlayerUuid;
extern ClientInstance* g_clientInstance;
extern Vec3 g_prevPhysicsPos;
extern Vec3 g_currPhysicsPos;
extern std::chrono::steady_clock::time_point g_lastPhysicsTime;
extern int g_playerBlockX;
extern int g_playerBlockZ;

// 数据并发锁，消灭画面撕裂
inline std::mutex g_mapDataMutex;
// 游戏线程完成一轮洞穴局部光照采样后，由 DX11 线程只重烘焙对应纹理矩形。
inline std::atomic<bool> g_caveLightRefreshReady{false};
inline int g_caveLightRefreshLeft = 0;
inline int g_caveLightRefreshTop = 0;
inline int g_caveLightRefreshRight = 0;
inline int g_caveLightRefreshBottom = 0;

// ==========================================
// 生物群系中文翻译字典引擎
// ==========================================
inline std::string TranslateBiomeName(const std::string& rawName) {
    if (LanguageManager::g_currentLanguage != "zh_CN" && LanguageManager::g_currentLanguage != "zh_TW") {
        std::string formattedName = rawName;
        for (size_t i = 0; i < formattedName.length(); ++i) {
            if (formattedName[i] == '_') formattedName[i] = ' ';
            if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
        }
        return formattedName;
    }

    std::string lower = rawName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;
    
    if (lower.find("plains") != std::string::npos && lower.find("ice") == std::string::npos && lower.find("snow") == std::string::npos) return "平原";
    if (lower.find("desert") != std::string::npos) return "沙漠";
    if (lower.find("extreme_hills") != std::string::npos || lower.find("windswept") != std::string::npos) return "风啸山丘";
    if (lower.find("forest") != std::string::npos && lower.find("dark") == std::string::npos && lower.find("birch") == std::string::npos && lower.find("crimson") == std::string::npos && lower.find("warped") == std::string::npos) return "森林";
    if (lower.find("taiga") != std::string::npos) return "针叶林";
    if (lower.find("swamp") != std::string::npos) {
        if (lower.find("mangrove") != std::string::npos) return "红树林沼泽";
        return "沼泽";
    }
    if (lower.find("river") != std::string::npos) return "河流";
    if (lower.find("hell") != std::string::npos || lower.find("nether") != std::string::npos) return "下界荒野";
    if (lower.find("the_end") != std::string::npos) return "末地";
    if (lower.find("ocean") != std::string::npos) {
        if (lower.find("frozen") != std::string::npos) return "冻洋";
        if (lower.find("warm") != std::string::npos) return "暖洋";
        if (lower.find("cold") != std::string::npos) return "冷洋";
        return "海洋";
    }
    if (lower.find("ice_plains") != std::string::npos || lower.find("snowy_plains") != std::string::npos) return "积雪平原";
    if (lower.find("ice_spikes") != std::string::npos) return "冰刺平原";
    if (lower.find("mushroom") != std::string::npos) return "蘑菇岛";
    if (lower.find("beach") != std::string::npos) return "海滩";
    if (lower.find("jungle") != std::string::npos) {
        if (lower.find("bamboo") != std::string::npos) return "竹林";
        return "丛林";
    }
    if (lower.find("birch_forest") != std::string::npos) return "白桦森林";
    if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos) return "黑橡木森林";
    if (lower.find("savanna") != std::string::npos) return "热带草原";
    if (lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) return "恶地";
    if (lower.find("cherry") != std::string::npos) return "樱花树林";
    if (lower.find("crimson_forest") != std::string::npos) return "绯红森林";
    if (lower.find("warped_forest") != std::string::npos) return "诡异森林";
    if (lower.find("soul_sand_valley") != std::string::npos) return "灵魂沙峡谷";
    if (lower.find("basalt_deltas") != std::string::npos) return "玄武岩三角洲";
    if (lower.find("meadow") != std::string::npos) return "草甸";
    if (lower.find("grove") != std::string::npos) return "雪林";
    if (lower.find("snowy_slopes") != std::string::npos) return "积雪的山坡";
    if (lower.find("jagged_peaks") != std::string::npos) return "尖峭山峰";
    if (lower.find("frozen_peaks") != std::string::npos) return "冰封山峰";
    if (lower.find("stony_peaks") != std::string::npos) return "裸岩山峰";
    if (lower.find("deep_dark") != std::string::npos) return "深暗之域";
    if (lower.find("pale_garden") != std::string::npos) return "苍白花园";
    
    return "未知群系";
}

// ==========================================
// 真彩自然光色彩引擎
// ==========================================
inline void getBiomeTints(std::string const& biomeName, mce::Color& grass, mce::Color& foliage, mce::Color& water) {
    grass   = mce::Color(0.32f, 0.45f, 0.22f, 1.0f);
    foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    water   = mce::Color(0.18f, 0.38f, 0.85f, 1.0f);

    if (biomeName.empty()) return;

    std::string lower = biomeName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.44f, 0.56f, 0.26f, 1.0f);
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
    }
    else if (lower.find("desert") != std::string::npos || lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) {
        grass   = mce::Color(0.45f, 0.42f, 0.20f, 1.0f);
        foliage = mce::Color(0.35f, 0.38f, 0.18f, 1.0f);
        water   = mce::Color(0.15f, 0.45f, 0.45f, 1.0f);
    } 
    else if (lower.find("savanna") != std::string::npos) {
        grass   = mce::Color(0.42f, 0.42f, 0.18f, 1.0f);
        foliage = mce::Color(0.32f, 0.35f, 0.15f, 1.0f);
    } 
    else if (lower.find("jungle") != std::string::npos || lower.find("bamboo") != std::string::npos) {
        grass   = mce::Color(0.32f, 0.50f, 0.18f, 1.0f);
        foliage = mce::Color(0.22f, 0.45f, 0.15f, 1.0f);
    } 
    else if (lower.find("swamp") != std::string::npos || lower.find("mangrove") != std::string::npos) {
        grass   = mce::Color(0.22f, 0.28f, 0.16f, 1.0f);
        foliage = mce::Color(0.18f, 0.25f, 0.15f, 1.0f);
        water   = mce::Color(0.15f, 0.25f, 0.22f, 1.0f);
    } 
    else if (lower.find("taiga") != std::string::npos || lower.find("snow") != std::string::npos || lower.find("ice") != std::string::npos || lower.find("frozen") != std::string::npos) {
        grass   = mce::Color(0.26f, 0.38f, 0.30f, 1.0f);
        foliage = mce::Color(0.15f, 0.28f, 0.25f, 1.0f);
    } 
    else if (lower.find("birch") != std::string::npos) {
        grass   = mce::Color(0.36f, 0.48f, 0.25f, 1.0f);
        foliage = mce::Color(0.25f, 0.42f, 0.20f, 1.0f);
    }
    else if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos) {
        grass   = mce::Color(0.20f, 0.30f, 0.12f, 1.0f);
        foliage = mce::Color(0.15f, 0.25f, 0.10f, 1.0f);
    }
    else if (lower.find("meadow") != std::string::npos || lower.find("grove") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.42f, 0.28f, 1.0f);
        foliage = mce::Color(0.18f, 0.32f, 0.20f, 1.0f);
    }
}

inline mce::Color getBlockColor(std::string const& name, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol) {
    if (name == "minecraft:air" || name == "air" || name.find("barrier") != std::string::npos ||
        name.find("light_block") != std::string::npos || name.find("structure_void") != std::string::npos ||
        name.find("placeholder") != std::string::npos || name.find("unknown") != std::string::npos ||
        name.find("info_update") != std::string::npos) {
        return mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (name.find("glass") != std::string::npos) return mce::Color(0.8f, 0.9f, 0.9f, 0.3f);
    if (name.find("path") != std::string::npos || name.find("farmland") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("bamboo") != std::string::npos) return mce::Color(0.40f, 0.70f, 0.20f, 1.0f);

    if (name.find("water") != std::string::npos) return waterCol;
    if (name.find("pink_petals") != std::string::npos) return mce::Color(0.95f, 0.68f, 0.78f, 1.0f);

    if (name.find("peony") != std::string::npos || name.find("pink_tulip") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (name.find("dandelion") != std::string::npos || name.find("sunflower") != std::string::npos || name.find("yellow_flower") != std::string::npos) return mce::Color(0.95f, 0.85f, 0.20f, 1.0f);
    if (name.find("rose") != std::string::npos || name.find("poppy") != std::string::npos || name.find("red_flower") != std::string::npos || name.find("red_tulip") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("orchid") != std::string::npos || name.find("cornflower") != std::string::npos) return mce::Color(0.20f, 0.40f, 0.85f, 1.0f);
    if (name.find("allium") != std::string::npos || name.find("lilac") != std::string::npos) return mce::Color(0.70f, 0.30f, 0.70f, 1.0f);
    if (name.find("daisy") != std::string::npos || name.find("bluet") != std::string::npos || name.find("valley") != std::string::npos || name.find("white_tulip") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (name.find("flower") != std::string::npos || name.find("bloom") != std::string::npos || name.find("blossom") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.25f, 1.0f);

    if (name.find("red_sand") != std::string::npos) return mce::Color(0.85f, 0.43f, 0.30f, 1.0f);

    if (name.find("white_") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (name.find("orange_") != std::string::npos) return mce::Color(0.85f, 0.50f, 0.20f, 1.0f);
    if (name.find("magenta_") != std::string::npos) return mce::Color(0.75f, 0.35f, 0.75f, 1.0f);
    if (name.find("light_blue_") != std::string::npos) return mce::Color(0.40f, 0.65f, 0.90f, 1.0f);
    if (name.find("yellow_") != std::string::npos) return mce::Color(0.90f, 0.85f, 0.20f, 1.0f);
    if (name.find("lime_") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.20f, 1.0f);
    if (name.find("pink_") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (name.find("gray_") != std::string::npos) return mce::Color(0.40f, 0.40f, 0.40f, 1.0f);
    if (name.find("light_gray_") != std::string::npos || name.find("silver_") != std::string::npos) return mce::Color(0.65f, 0.65f, 0.65f, 1.0f);
    if (name.find("cyan_") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.60f, 1.0f);
    if (name.find("purple_") != std::string::npos) return mce::Color(0.50f, 0.25f, 0.60f, 1.0f);
    if (name.find("blue_") != std::string::npos) return mce::Color(0.20f, 0.30f, 0.70f, 1.0f);
    if (name.find("brown_") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);
    if (name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.50f, 0.20f, 1.0f);
    if (name.find("red_") != std::string::npos) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    if (name.find("black_") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.15f, 1.0f);

    if (name.find("double_plant") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);

    if (name.find("lily_pad") != std::string::npos || name.find("waterlily") != std::string::npos) return mce::Color(0.25f, 0.55f, 0.20f, 1.0f);

    if (name.find("mushroom") != std::string::npos || name.find("fungus") != std::string::npos || name.find("fungi") != std::string::npos) {
        if (name.find("red") != std::string::npos || name.find("crimson") != std::string::npos) return mce::Color(0.85f, 0.20f, 0.20f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
        if (name.find("warped") != std::string::npos) return mce::Color(0.15f, 0.55f, 0.50f, 1.0f);
        return mce::Color(0.80f, 0.70f, 0.60f, 1.0f);
    }

    if (name.find("wart") != std::string::npos) return mce::Color(0.65f, 0.10f, 0.10f, 1.0f);
    if (name.find("chorus") != std::string::npos) return mce::Color(0.60f, 0.40f, 0.60f, 1.0f);
    if (name.find("purpur") != std::string::npos) return mce::Color(0.66f, 0.48f, 0.64f, 1.0f);

    if (name.find("grass") != std::string::npos || name.find("fern") != std::string::npos || name.find("moss") != std::string::npos ||
        name.find("shrub") != std::string::npos || name.find("plant") != std::string::npos || name.find("vine") != std::string::npos ||
        name.find("sapling") != std::string::npos || name.find("propagule") != std::string::npos || name.find("sugar_cane") != std::string::npos ||
        name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos || name.find("potato") != std::string::npos ||
        name.find("beetroot") != std::string::npos || name.find("crop") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("bush") != std::string::npos || name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos ||
        name.find("lichen") != std::string::npos || name.find("seed") != std::string::npos) {
        return grassCol;
    }
    if (name.find("dirt") != std::string::npos || name.find("podzol") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);

    if (name.find("pumpkin") != std::string::npos || name.find("melon") != std::string::npos) {
        if (name.find("melon") != std::string::npos) return mce::Color(0.50f, 0.65f, 0.15f, 1.0f);
        return mce::Color(0.90f, 0.45f, 0.05f, 1.0f);
    }

    if (name.find("end_stone") != std::string::npos) return mce::Color(0.86f, 0.89f, 0.65f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.60f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("glowstone") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.30f, 1.0f);
    if (name.find("lodestone") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
    if (name.find("end_portal") != std::string::npos) return mce::Color(0.10f, 0.25f, 0.25f, 1.0f);

    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("lava") != std::string::npos) return mce::Color(0.95f, 0.30f, 0.0f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.18f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("quartz_ore") != std::string::npos) return mce::Color(0.65f, 0.55f, 0.55f, 1.0f);
    if (name.find("nether_gold_ore") != std::string::npos) return mce::Color(0.65f, 0.35f, 0.15f, 1.0f);

    if (name.find("fallen") != std::string::npos || name.find("dead") != std::string::npos ||
        name.find("litter") != std::string::npos || name.find("brown") != std::string::npos) {
        if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) {
            return mce::Color(0.55f, 0.38f, 0.20f, 1.0f);
        }
    }

    if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos || name.find("azalea") != std::string::npos) {
        if (name.find("cherry") != std::string::npos) return mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
        if (name.find("mangrove") != std::string::npos) return mce::Color(0.15f, 0.30f, 0.10f, 1.0f);
        return foliageCol;
    }

    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.95f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.65f, 0.85f, 0.95f, 1.0f);
    if (name.find("snow") != std::string::npos) return mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.28f, 0.22f, 0.16f, 1.0f); // 灵魂土（比灵魂沙略深）
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.36f, 0.27f, 0.18f, 1.0f); // 灵魂沙（与灵魂土相近，略浅）
    if (name.find("sand") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.60f, 1.0f);

    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white_") != std::string::npos) return mce::Color(0.82f, 0.70f, 0.63f, 1.0f);
        if (name.find("orange_") != std::string::npos) return mce::Color(0.63f, 0.33f, 0.14f, 1.0f);
        if (name.find("magenta_") != std::string::npos) return mce::Color(0.58f, 0.34f, 0.42f, 1.0f);
        if (name.find("light_blue_") != std::string::npos) return mce::Color(0.44f, 0.42f, 0.53f, 1.0f);
        if (name.find("yellow_") != std::string::npos) return mce::Color(0.73f, 0.52f, 0.11f, 1.0f);
        if (name.find("lime_") != std::string::npos) return mce::Color(0.41f, 0.46f, 0.20f, 1.0f);
        if (name.find("pink_") != std::string::npos) return mce::Color(0.63f, 0.30f, 0.30f, 1.0f);
        if (name.find("gray_") != std::string::npos) return mce::Color(0.22f, 0.16f, 0.15f, 1.0f);
        if (name.find("light_gray_") != std::string::npos || name.find("silver") != std::string::npos) return mce::Color(0.53f, 0.42f, 0.38f, 1.0f);
        if (name.find("cyan_") != std::string::npos) return mce::Color(0.34f, 0.35f, 0.35f, 1.0f);
        if (name.find("purple_") != std::string::npos) return mce::Color(0.46f, 0.28f, 0.33f, 1.0f);
        if (name.find("blue_") != std::string::npos) return mce::Color(0.29f, 0.23f, 0.35f, 1.0f);
        if (name.find("brown_") != std::string::npos) return mce::Color(0.30f, 0.20f, 0.13f, 1.0f);
        if (name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.33f, 0.15f, 1.0f);
        if (name.find("red_") != std::string::npos) return mce::Color(0.56f, 0.24f, 0.18f, 1.0f);
        if (name.find("black_") != std::string::npos) return mce::Color(0.14f, 0.09f, 0.08f, 1.0f);
        return mce::Color(0.59f, 0.35f, 0.22f, 1.0f);
    }

    if (name.find("obsidian") != std::string::npos) return mce::Color(0.10f, 0.10f, 0.18f, 1.0f);

    if (name.find("deepslate") != std::string::npos && name.find("ore") == std::string::npos) return mce::Color(0.25f, 0.25f, 0.28f, 1.0f);

    if (name.find("copper") != std::string::npos) {
        if (name.find("oxidized") != std::string::npos) return mce::Color(0.36f, 0.68f, 0.49f, 1.0f);
        if (name.find("weathered") != std::string::npos) return mce::Color(0.42f, 0.66f, 0.60f, 1.0f);
        if (name.find("exposed") != std::string::npos) return mce::Color(0.55f, 0.71f, 0.63f, 1.0f);
        if (name.find("raw_") != std::string::npos) return mce::Color(0.77f, 0.54f, 0.35f, 1.0f);
        if (name.find("ore") != std::string::npos) {
            if (name.find("deepslate") != std::string::npos) return mce::Color(0.45f, 0.35f, 0.30f, 1.0f);
            return mce::Color(0.77f, 0.49f, 0.35f, 1.0f);
        }
        return mce::Color(0.88f, 0.49f, 0.24f, 1.0f); // 普通铜块
    }

    // 其他矿石
    if (name.find("coal_ore") != std::string::npos) return mce::Color(0.20f, 0.20f, 0.20f, 1.0f);
    if (name.find("iron_ore") != std::string::npos) return mce::Color(0.75f, 0.60f, 0.55f, 1.0f);
    if (name.find("gold_ore") != std::string::npos) return mce::Color(0.85f, 0.70f, 0.25f, 1.0f);
    if (name.find("diamond_ore") != std::string::npos) return mce::Color(0.45f, 0.75f, 0.80f, 1.0f);
    if (name.find("emerald_ore") != std::string::npos) return mce::Color(0.35f, 0.75f, 0.35f, 1.0f);
    if (name.find("lapis_ore") != std::string::npos) return mce::Color(0.35f, 0.40f, 0.70f, 1.0f);

    // 粗矿块（raw_copper_block 已在 copper 段处理）
    if (name.find("raw_iron") != std::string::npos) return mce::Color(0.72f, 0.55f, 0.45f, 1.0f);
    if (name.find("raw_gold") != std::string::npos) return mce::Color(0.85f, 0.65f, 0.30f, 1.0f);

    // 金属块（raw_*_block 类必须在前面检查）
    if (name.find("iron_block") != std::string::npos) return mce::Color(0.80f, 0.80f, 0.82f, 1.0f);
    if (name.find("gold_block") != std::string::npos) return mce::Color(0.92f, 0.78f, 0.18f, 1.0f);
    if (name.find("diamond_block") != std::string::npos) return mce::Color(0.30f, 0.75f, 0.85f, 1.0f);
    if (name.find("emerald_block") != std::string::npos) return mce::Color(0.25f, 0.75f, 0.30f, 1.0f);
    if (name.find("lapis_block") != std::string::npos) return mce::Color(0.15f, 0.25f, 0.55f, 1.0f);
    if (name.find("netherite_block") != std::string::npos) return mce::Color(0.22f, 0.18f, 0.18f, 1.0f);
    if (name.find("coal_block") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.12f, 1.0f);

    // 深暗之域（Sculk）
    if (name.find("sculk") != std::string::npos) return mce::Color(0.10f, 0.18f, 0.16f, 1.0f);

    // 紫晶
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.55f, 0.30f, 0.65f, 1.0f);

    // 水滴石 / 方解石
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.50f, 0.40f, 0.30f, 1.0f);
    if (name.find("calcite") != std::string::npos) return mce::Color(0.90f, 0.90f, 0.88f, 1.0f);

    // 基岩 / 刷怪笼
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.12f, 1.0f);
    if (name.find("spawner") != std::string::npos) return mce::Color(0.20f, 0.14f, 0.24f, 1.0f);

    // 泥巴 / 红树
    if (name.find("mud") != std::string::npos || name.find("mangrove") != std::string::npos) return mce::Color(0.30f, 0.22f, 0.15f, 1.0f);

    // 海晶石
    if (name.find("prismarine") != std::string::npos) {
        if (name.find("dark") != std::string::npos) return mce::Color(0.15f, 0.35f, 0.25f, 1.0f);
        return mce::Color(0.30f, 0.55f, 0.45f, 1.0f);
    }
    if (name.find("sea_lantern") != std::string::npos) return mce::Color(0.90f, 0.85f, 0.65f, 1.0f);
    if (name.find("conduit") != std::string::npos) return mce::Color(0.35f, 0.55f, 0.75f, 1.0f);
    if (name.find("sponge") != std::string::npos) {
        if (name.find("wet") != std::string::npos) return mce::Color(0.70f, 0.65f, 0.30f, 1.0f);
        return mce::Color(0.85f, 0.75f, 0.20f, 1.0f);
    }

    // 下界
    // 灵魂沙/灵魂土颜色已在上方 sand 规则前单独处理

    // 功能方块
    if (name.find("crafting_table") != std::string::npos) return mce::Color(0.55f, 0.30f, 0.15f, 1.0f);
    if (name.find("furnace") != std::string::npos) return mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
    if (name == "minecraft:tnt" || name == "tnt") return mce::Color(0.75f, 0.15f, 0.15f, 1.0f);

    // 蜂巢 / 蜂蜜
    if (name.find("bee_nest") != std::string::npos || name.find("beehive") != std::string::npos) return mce::Color(0.65f, 0.50f, 0.20f, 1.0f);
    if (name.find("honeycomb") != std::string::npos) return mce::Color(0.85f, 0.55f, 0.15f, 1.0f);
    if (name.find("honey") != std::string::npos) return mce::Color(0.85f, 0.60f, 0.20f, 1.0f);

    if (name.find("planks") != std::string::npos || name.find("oak") != std::string::npos || name.find("spruce") != std::string::npos || name.find("birch") != std::string::npos || name.find("jungle") != std::string::npos || name.find("acacia") != std::string::npos || name.find("dark_oak") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos || name.find("stem") != std::string::npos || name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos || name.find("fence") != std::string::npos || name.find("door") != std::string::npos || name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos || name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("stone") != std::string::npos || name.find("cobble") != std::string::npos || name.find("andesite") != std::string::npos || name.find("diorite") != std::string::npos || name.find("granite") != std::string::npos || name.find("tuff") != std::string::npos || name.find("brick") != std::string::npos || name.find("wall") != std::string::npos || name.find("gravel") != std::string::npos || name.find("clay") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    if (name.find("deepslate") != std::string::npos) return mce::Color(0.30f, 0.28f, 0.32f, 1.0f);

    unsigned int h = 0;
    for (char c : name) h = h * 31 + c;
    float r = 0.45f + ((h >> 16) & 0xFF) / 255.0f * 0.4f;
    float g = 0.45f + ((h >> 8) & 0xFF) / 255.0f * 0.4f;
    float b = 0.45f + (h & 0xFF) / 255.0f * 0.4f;
    return mce::Color(r, g, b, 1.0f);
}

// Xaero 风格洞穴分层投影：所有列从同一个 Top Y 向下解析，避免相邻列跳到不同高度层。
inline constexpr int kCaveLayerTopOffset = 3;
inline constexpr int kCaveLayerAirSearchDepth = 64;
inline constexpr int kCaveLayerFloorSearchDepth = 64;
inline constexpr int kCaveOpenWaterSkySearchRange = 192;

using MaterialType = SharedTypes::v1_26_20::MaterialType;

inline MaterialType GetCaveMaterialType(Block const& block) {
    return block.getBlockType().mMaterial.mType;
}

inline bool IsCaveWaterBlock(Block const& block) {
    if (block.isAir()) return false;
    MaterialType material = GetCaveMaterialType(block);
    return material == MaterialType::Water || material == MaterialType::Bubble;
}

inline constexpr float kWaterOverlayAlpha = 0.65f;
inline constexpr mce::Color kDefaultWaterTint(0.18f, 0.38f, 0.85f, 1.0f);

// 水色叠加在海床/洞底颜色之上；高度阴影仍由后续烘焙使用底部 Y 计算。
inline mce::Color BlendWaterOverFloor(mce::Color floorColor, mce::Color waterTint) {
    if (waterTint.a <= 0.01f) waterTint = kDefaultWaterTint;
    return mce::Color(
        floorColor.r + (waterTint.r - floorColor.r) * kWaterOverlayAlpha,
        floorColor.g + (waterTint.g - floorColor.g) * kWaterOverlayAlpha,
        floorColor.b + (waterTint.b - floorColor.b) * kWaterOverlayAlpha,
        floorColor.a
    );
}

// 洞穴投影和自动判定共享此规则：流体、植被与透明方块都不形成洞穴墙体。
inline bool IsCavePassableBlock(Block const& block) {
    if (block.isAir()) return true;

    MaterialType material = GetCaveMaterialType(block);
    switch (material) {
    case MaterialType::Air:
    case MaterialType::Water:
    case MaterialType::Bubble:
    case MaterialType::Plant:
    case MaterialType::SolidPlant:
    case MaterialType::Leaves:
    case MaterialType::Glass:
    case MaterialType::Ice:
    case MaterialType::PowderSnow:
    case MaterialType::Cactus:
    case MaterialType::Fire:
    case MaterialType::Portal:
    case MaterialType::Grate:
    case MaterialType::StoneDecoration:
    case MaterialType::DecorationSolid:
    case MaterialType::NonSolid:
    case MaterialType::StructureVoid:
        return true;
    default:
        break;
    }

    if (material == MaterialType::Wood) {
        std::string const& name = block.getTypeName();
        return name.find("log") != std::string::npos || name.find("stem") != std::string::npos;
    }
    return false;
}

// 仅在玩家附近已经发现水体时调用。水或气泡经开放空间连到天空则是海洋/湖泊，而非洞穴。
inline bool HasOpenWaterRouteToSky(BlockSource& region, int x, int y, int z) {
    bool sawWater = false;
    for (int distance = 0; distance <= kCaveOpenWaterSkySearchRange; ++distance) {
        int candidateY = y + distance;
        if (candidateY > 319) return sawWater;

        Block const& block = region.getBlock(BlockPos(x, candidateY, z));
        if (IsCaveWaterBlock(block)) sawWater = true;
        if (!IsCavePassableBlock(block)) return false;
    }
    return sawWater;
}

// 自动检测玩家是否在地下：顶棚、下方地板和天空光共同避免大型封闭洞室误判为地表。
inline bool IsPlayerUnderground(BlockSource* region, int px, int py, int pz) {
    if (!region) return false;
    constexpr int kVerticalRange = 20;
    constexpr int kMinWorldY = -64;
    constexpr int kMaxWorldY = 319;
    constexpr int kLowSkyLight = 1;
    int enclosedColumns = 0;
    int offsets[5][2] = {{0,0}, {1,0}, {-1,0}, {0,1}, {0,-1}};

    for (auto& off : offsets) {
        int sampleX = px + off[0];
        int sampleZ = pz + off[1];
        int sampleY = std::clamp(py, kMinWorldY, kMaxWorldY);
        bool hasCeiling = false;
        bool hasFloor = false;
        bool hasWaterAbove = false;
        int skyLight = 15;

        try {
            BlockPos lightPos(sampleX, std::clamp(sampleY + 1, kMinWorldY, kMaxWorldY), sampleZ);
            if (!region->hasChunksAt(lightPos, 0, false)) continue;
            skyLight = region->getSkylightBrightness(lightPos);
            hasWaterAbove = IsCaveWaterBlock(region->getBlock(BlockPos(sampleX, sampleY, sampleZ))) ||
                IsCaveWaterBlock(region->getBlock(lightPos));

            for (int distance = 1; distance <= kVerticalRange && (!hasCeiling || !hasFloor); ++distance) {
                int upperY = sampleY + distance;
                if (upperY <= kMaxWorldY) {
                    Block const& upperBlock = region->getBlock(BlockPos(sampleX, upperY, sampleZ));
                    hasWaterAbove = hasWaterAbove || IsCaveWaterBlock(upperBlock);
                    if (!hasCeiling && !IsCavePassableBlock(upperBlock)) {
                        hasCeiling = true;
                    }
                }

                int lowerY = sampleY - distance;
                if (!hasFloor && lowerY >= kMinWorldY) {
                    Block const& lowerBlock = region->getBlock(BlockPos(sampleX, lowerY, sampleZ));
                    if (!IsCavePassableBlock(lowerBlock)) {
                        hasFloor = true;
                    }
                }
            }
        } catch (...) {
            continue;
        }

        bool openWaterRoute = hasWaterAbove && HasOpenWaterRouteToSky(*region, sampleX, sampleY, sampleZ);
        // 水列已向上确认到 192 格；遇到实体顶棚即使超过常规 20 格，也应保留含水洞穴模式。
        bool hasWaterCeiling = hasWaterAbove && !openWaterRoute;
        if (hasCeiling || hasWaterCeiling || (hasFloor && skyLight <= kLowSkyLight && !openWaterRoute)) {
            ++enclosedColumns;
        }
    }
    return enclosedColumns >= 3;
}

inline void HandleClientInstanceUpdate(ClientInstance* clientInstance, bool isInitFinished) {
    if (!clientInstance) return;
    MapRenderState::lastFrameTotalCalls = MapRenderState::frameCallCount.load();
    if (MapRenderState::lastFrameTotalCalls < 1) MapRenderState::lastFrameTotalCalls = 1;
    MapRenderState::frameCallCount.store(0);

    g_clientInstance = clientInstance;

    auto* player = clientInstance->getLocalPlayer();
        if (player && clientInstance->isWorldActive()) {
            const Vec3 pos = player->getFeetPos();

            if (g_prevPhysicsPos.x == 0.0f && g_prevPhysicsPos.z == 0.0f) {
                g_prevPhysicsPos = pos;
            } else {
                g_prevPhysicsPos = g_currPhysicsPos;
            }
            g_currPhysicsPos  = pos;
            g_lastPhysicsTime = std::chrono::steady_clock::now();

            g_playerBlockX = (int)std::floor(pos.x);
            g_playerBlockZ = (int)std::floor(pos.z);
            g_playerX = pos.x; g_playerY = pos.y; g_playerZ = pos.z;
            g_playerYaw = player->getRotation().y;
            g_hasPlayer   = true;
            g_localPlayer = player;
            {
                std::lock_guard<std::mutex> lock(g_radarMutex);
                g_localPlayerUuid = static_cast<std::string>(player->getUuid());
            }

        // ==========================================
        // [防窒息 & 神躯护体] 智能高度推算与无敌降落引擎
        // ==========================================
        if (MapRenderState::triggerTeleport.load()) {
            float targetX = MapRenderState::tpTargetX;
            float targetY = MapRenderState::tpTargetY;
            float targetZ = MapRenderState::tpTargetZ;
            
            bool needsSafeFall = false;

            if (targetY < -500.0f) {
                BlockSource* region = clientInstance->getRegion();
                if (region) {
                    short topY = region->getAboveTopSolidBlock((int)targetX, (int)targetZ, true, true);
                    if (topY > -60 && topY < 319) {
                        targetY = (float)topY + 1.0f; 
                    } else {
                        targetY = 320.0f; 
                        needsSafeFall = true; 
                    }
                } else {
                    targetY = 320.0f;
                    needsSafeFall = true;
                }
            } else if (std::abs(targetY - 320.0f) < 0.1f) {
                // 【完成需求3】仅当精确传送到高空 Y=320 时，才触发无敌抗性护体
                needsSafeFall = true;
            }

            std::thread([needsSafeFall, targetX, targetY, targetZ]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                
                auto sendCommand = [](const char* cmd) {
                    bool clipSuccess = false;
                    for (int i = 0; i < 5; ++i) { 
                        if (OpenClipboard(nullptr)) {
                            EmptyClipboard();
                            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, std::strlen(cmd) + 1);
                            if (hMem) {
                                std::memcpy(GlobalLock(hMem), cmd, std::strlen(cmd) + 1);
                                GlobalUnlock(hMem);
                                SetClipboardData(CF_TEXT, hMem);
                            }
                            CloseClipboard();
                            clipSuccess = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    if (!clipSuccess) return;

                    keybd_event(VK_RETURN, 0, 0, 0); keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
                    
                    keybd_event(VK_CONTROL, 0, 0, 0); keybd_event('A', 0, 0, 0);
                    keybd_event('A', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    keybd_event(VK_BACK, 0, 0, 0); keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));

                    keybd_event(VK_CONTROL, 0, 0, 0); keybd_event('V', 0, 0, 0);
                    keybd_event('V', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    
                    keybd_event(VK_RETURN, 0, 0, 0); keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
                };

                if (needsSafeFall) {
                    char cmd1[128];
                    snprintf(cmd1, sizeof(cmd1), "/effect @s resistance 30 255 true");
                    sendCommand(cmd1);
                }

                char cmd2[128];
                snprintf(cmd2, sizeof(cmd2), "/tp @s %d %d %d", (int)targetX, (int)targetY, (int)targetZ);
                sendCommand(cmd2);
                
            }).detach();
            
            MapRenderState::triggerTeleport.store(false);
        }

        // ==========================================
        // [分层地图系统] 跨维度与跨地下层级监听器
        // ==========================================
        try {
            int dimId = (int)player->getDimensionId();
            std::string rawLevelId = "UnknownWorld";
            std::string seedStr = "0";
            std::string spawnStr = "0_0";

            try { 
                std::string lid = player->getLevel().getLevelId();
                if (!lid.empty()) rawLevelId = lid;
            } catch(...) {}
            
            try { 
                unsigned int seed = player->getLevel().getSeed();
                seedStr = std::to_string(seed);
            } catch(...) {}

            try { 
                BlockPos spawn = player->getLevel().getDefaultSpawn();
                spawnStr = std::to_string(spawn.x) + "_" + std::to_string(spawn.z);
            } catch(...) {}

            if (rawLevelId.empty() || rawLevelId == "UnknownWorld") {
                rawLevelId = "RemoteServer";
            }
            
            std::string finalWorldId = rawLevelId + "_S" + seedStr + "_P" + spawnStr;

            for (char& c : finalWorldId) {
                if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
            }
            
            if (MapRenderState::currentWorldId != finalWorldId || MapRenderState::currentDimensionId != dimId) {
                MapRenderState::currentWorldId = finalWorldId;
                MapRenderState::currentDimensionId = dimId;
                MapRenderState::worldSwitchTime = std::chrono::steady_clock::now();
                
                MapCacheManager::SwitchWorld(finalWorldId, dimId);
                WaypointManager::SwitchWorld(finalWorldId, dimId); 
                DeathPointManager::SwitchWorld(finalWorldId);
                
                std::memset(g_mapHeights, 0, sizeof(g_mapHeights));
                std::memset(g_mapColors, 0, sizeof(g_mapColors));
                std::memset(g_mapWaterFlags, 0, sizeof(g_mapWaterFlags));
                std::memset(g_mapBrightness, 0, sizeof(g_mapBrightness));
                std::memset(g_mapChannelHeights, 0, sizeof(g_mapChannelHeights));
                std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
                std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
                std::memset(g_mapWaterFlagsBack, 0, sizeof(g_mapWaterFlagsBack));
                std::memset(g_mapBrightnessBack, 0, sizeof(g_mapBrightnessBack));
                std::memset(g_mapChannelHeightsBack, 0, sizeof(g_mapChannelHeightsBack));
                {
                    std::lock_guard<std::mutex> lock(g_radarMutex);
                    g_radarEntities.clear();
                    g_localPlayerUuid.clear();
                }
                g_radarGeneration.fetch_add(1, std::memory_order_release);
                {
                    std::lock_guard<std::mutex> lock(g_playerSkinMutex);
                    g_playerSkinHeads.clear();
                }
                
                // 下界自动洞穴模式
                if (dimId == 1) {
                    MapRenderState::caveMode = true;
                    MapRenderState::caveScanY = std::clamp(
                        (int)std::floor(g_playerY) + kCaveLayerTopOffset,
                        -64,
                        319
                    );
                } else {
                    MapRenderState::caveMode = false;
                }
                // 地表模式才预加载地表缓存；洞穴模式留空（虚空），避免显示地表颜色
                if (!MapRenderState::caveMode) {
                    MapCacheManager::PreloadScanBuffer(g_playerBlockX, g_playerBlockZ, g_mapColors, g_mapHeights);
                }
                g_lastRenderX = g_playerBlockX;
                g_lastRenderZ = g_playerBlockZ;
                
                MapRenderState::clearGPUCache.store(true); 
                MapRenderState::clearPlayerHeadTextures.store(true);
                g_mapDataGeneration.fetch_add(1);
                g_mapDataUpdated.store(true);
            }
        } catch(...) {}

        try {
            BlockSource* regionPtr = clientInstance->getRegion();
            if (regionPtr) {
                auto const& biome = regionPtr->getBiome(BlockPos(g_playerBlockX, (int)g_playerY, g_playerBlockZ));
                {
                    std::string rawName = biome.mHash->getString();
                    std::string translatedName = TranslateBiomeName(rawName);
                    
                    if (LanguageManager::g_currentLanguage == "zh_CN" || LanguageManager::g_currentLanguage == "zh_TW") {
                        std::string formattedName = rawName;
                        for (size_t i = 0; i < formattedName.length(); ++i) {
                            if (formattedName[i] == '_') formattedName[i] = ' ';
                            if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
                        }
                        MapRenderState::currentBiomeName = formattedName + " (" + translatedName + ")";
                    } else {
                        MapRenderState::currentBiomeName = translatedName;
                    }
                }
            }
        } catch (...) {}

        static bool wasPlayerAlive = true;
        static bool hasLastAlivePos = false;
        static int lastAliveX = 0;
        static int lastAliveY = 0;
        static int lastAliveZ = 0;
        static int lastAliveDimensionId = 0;

        bool isPlayerAlive = true;
        try {
            isPlayerAlive = player->isAlive();
        } catch (...) {
            isPlayerAlive = true;
        }

        if (isPlayerAlive) {
            wasPlayerAlive = true;
            hasLastAlivePos = true;
            lastAliveX = g_playerBlockX;
            lastAliveY = (int)std::floor(g_playerY);
            lastAliveZ = g_playerBlockZ;
            lastAliveDimensionId = MapRenderState::currentDimensionId;
        } else if (wasPlayerAlive && hasLastAlivePos && !MapRenderState::currentWorldId.empty()) {
            DeathPointManager::AddDeathPoint(lastAliveX, lastAliveY, lastAliveZ, lastAliveDimensionId);
            wasPlayerAlive = false;
        }

        static bool lastUIState = false;
        bool currentUIState = MapRenderState::IsUIActive();
        if (currentUIState != lastUIState) {
            lastUIState = currentUIState;
            HWND hwnd = FindWindowA("Bedrock", "Minecraft");
            if (!hwnd) hwnd = FindWindowW(L"Minecraft", NULL);
            if (!hwnd) hwnd = GetForegroundWindow();
            
            if (currentUIState) {
                clientInstance->releaseMouse();
                ClipCursor(NULL);
            } else {
                clientInstance->grabMouse();
                if (hwnd) {
                    RECT rect; GetWindowRect(hwnd, &rect);
                    SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
                }
            }
        }

        if (currentUIState) {
            ClipCursor(NULL);
        }

        HWND hwnd = FindWindowA("Bedrock", "Minecraft");
        if (!hwnd) hwnd = FindWindowW(L"Minecraft", NULL);
        if (!hwnd) hwnd = GetForegroundWindow();
        
        if (hwnd && GetForegroundWindow() == hwnd) {
            if (!MapRenderState::IsUIActive()) {
                CURSORINFO ci = {};
                ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == 0) {
                        RECT clientRect;
                        GetClientRect(hwnd, &clientRect);
                        
                        // 计算游戏客户区的正中心坐标
                        POINT ptCenter = { (clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2 };
                        ClientToScreen(hwnd, &ptCenter);
                        
                        // 【防覆盖层透传】将隐形指针死死锁在屏幕正中心 2x2 像素的极小死区内
                        // 彻底杜绝指针在疯狂转动视角时漂移到边缘的 Xbox Game Bar 等悬浮性能面板上导致游戏失焦
                        RECT centerRect = { ptCenter.x - 1, ptCenter.y - 1, ptCenter.x + 1, ptCenter.y + 1 };
                        ClipCursor(&centerRect);
                    } else {
                        // 处于背包、设置或原生游戏暂停菜单状态（系统鼠标显现），放开剪裁范围
                        ClipCursor(NULL);
                    }
                }
            }
        }

        static int currentScanX  = -99999;
        static int currentScanZ  = -99999;
        static int currentScanY  = -99999;
        static int currentCaveTopY = -99999;
        static bool isScanning   = false;
        static int  ticksSinceScan = 0;
        static bool prevCave     = false;
        struct ScanCell {
            short dx;
            short dz;
        };
        enum class ScanPhase {
            SurfaceVisible,
            SurfaceOuter,
            CaveSeedVisible,
            CaveResolveVisible,
            CaveSeedOuter,
            CaveResolveOuter
        };
        static std::vector<ScanCell> scanOrder;
        static size_t scanCursor = 0;
        static size_t visibleScanCellCount = 0;
        static ScanPhase scanPhase = ScanPhase::SurfaceVisible;
        static bool minimapSnapshotPublished = false;
        static bool hasPublishedScanBuffer = false;
        static bool publishedBufferWasCave = false;
        static size_t outerCellsSincePublish = 0;
        static bool visibleAreaHadUnloadedChunks = false;
        static std::string scanWorldId;
        static int scanWorldDimension = -99999;
        // 阶段一保存每列从 Top Y 向下首次命中空气的深度，阶段二无需重复读取空气列。
        static signed char caveChannelDrops[MAP_DATA_SIZE][MAP_DATA_SIZE] = {};
        static constexpr int kCaveChunkGridSize = MAP_DATA_SIZE / 16 + 2;
        static signed char caveChunkLoadStates[kCaveChunkGridSize][kCaveChunkGridSize] = {};
        static int caveChunkBaseX = 0;
        static int caveChunkBaseZ = 0;
        static int caveDetectionTicks = 0;
        static bool caveLightRefreshActive = false;
        static int caveLightRefreshIndex = 0;
        static int caveLightRefreshLeft = 0;
        static int caveLightRefreshTop = 0;
        static int caveLightRefreshCooldown = 0;

        int px = g_playerBlockX;
        int pz = g_playerBlockZ;
        int py = (int)std::floor(g_playerY);
        ticksSinceScan++;

        if (scanWorldId != MapRenderState::currentWorldId || scanWorldDimension != MapRenderState::currentDimensionId) {
            scanWorldId = MapRenderState::currentWorldId;
            scanWorldDimension = MapRenderState::currentDimensionId;
            currentScanX = currentScanZ = currentScanY = -99999;
            isScanning = false;
            hasPublishedScanBuffer = false;
            ticksSinceScan = 101;
        }

        auto publishScanBuffer = [&]() {
            std::lock_guard<std::mutex> lock(g_mapDataMutex);
            std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
            std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
            std::memcpy(g_mapWaterFlags, g_mapWaterFlagsBack, sizeof(g_mapWaterFlags));
            std::memcpy(g_mapBrightness, g_mapBrightnessBack, sizeof(g_mapBrightness));
            std::memcpy(g_mapChannelHeights, g_mapChannelHeightsBack, sizeof(g_mapChannelHeights));
            g_lastRenderX = currentScanX;
            g_lastRenderZ = currentScanZ;
            g_mapDataGeneration.fetch_add(1);
            g_mapDataUpdated.store(true);
        };

        // 洞穴判断不参与逐列扫描。扫描期间每 10 tick 复查一次，发现模式变化立刻废弃旧任务。
        if (!isScanning || ++caveDetectionTicks >= 10) {
            caveDetectionTicks = 0;
            bool detectedCave = false;
            if (MapRenderState::currentDimensionId == 1) {
                detectedCave = true;
            } else {
                auto* region = clientInstance->getRegion();
                detectedCave = region ? IsPlayerUnderground(region, px, py, pz) : false;
                auto sinceWorldSwitch = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - MapRenderState::worldSwitchTime
                ).count();
                if (sinceWorldSwitch < 6) detectedCave = false;
            }

            if (detectedCave != MapRenderState::caveMode) {
                MapRenderState::caveMode = detectedCave;
                if (!detectedCave) {
                    // 回到地表先恢复地表缓存；扫描完成前前台纹理不会被黑色后台覆盖。
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    MapCacheManager::PreloadScanBuffer(px, pz, g_mapColors, g_mapHeights, true);
                    std::fill(&g_mapBrightness[0][0], &g_mapBrightness[0][0] + MAP_DATA_SIZE * MAP_DATA_SIZE, 1.0f);
                    std::memset(g_mapChannelHeights, 0, sizeof(g_mapChannelHeights));
                    g_lastRenderX = px;
                    g_lastRenderZ = pz;
                    g_mapDataGeneration.fetch_add(1);
                    g_mapDataUpdated.store(true);
                }
            }
        }

        // 位置、高度或模式改变时，扫描中的数据不再有效，下一段扫描从新的任务参数重新开始。
        if (isScanning &&
            (std::abs(px - currentScanX) >= 16 || std::abs(pz - currentScanZ) >= 16 ||
             py != currentScanY || MapRenderState::caveMode != prevCave)) {
            isScanning = false;
            ticksSinceScan = 100;
        }

        if (!isScanning &&
            (std::abs(px - currentScanX) >= 16 || std::abs(pz - currentScanZ) >= 16 ||
             py != currentScanY || ticksSinceScan > 100)) {
            const int previousRenderX = g_lastRenderX;
            const int previousRenderZ = g_lastRenderZ;
            currentScanX  = px;
            currentScanZ  = pz;
            currentScanY  = py;
            currentCaveTopY = std::clamp(
                currentScanY + kCaveLayerTopOffset,
                -64,
                319
            );
            ticksSinceScan = 0;
            prevCave = MapRenderState::caveMode;
            MapRenderState::caveScanY = prevCave ? currentCaveTopY : currentScanY;

            const int shiftX = currentScanX - previousRenderX;
            const int shiftZ = currentScanZ - previousRenderZ;
            const bool canReuseFrontBuffer = hasPublishedScanBuffer &&
                publishedBufferWasCave == prevCave &&
                std::abs(shiftX) < MAP_DATA_SIZE && std::abs(shiftZ) < MAP_DATA_SIZE;
            std::vector<unsigned char> newEdgeCells(MAP_DATA_SIZE * MAP_DATA_SIZE, 0);

            {
                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
                std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
                std::memset(g_mapWaterFlagsBack, 0, sizeof(g_mapWaterFlagsBack));
                std::fill(&g_mapBrightnessBack[0][0], &g_mapBrightnessBack[0][0] + MAP_DATA_SIZE * MAP_DATA_SIZE, prevCave ? -1.0f : 1.0f);
                std::memset(g_mapChannelHeightsBack, 0, sizeof(g_mapChannelHeightsBack));
                if (!prevCave) {
                    MapCacheManager::PreloadScanBuffer(currentScanX, currentScanZ, g_mapColorsBack, g_mapHeightsBack);
                }

                // 重心移动时复用旧窗口，只让新露出的边缘成为优先扫描目标。
                if (canReuseFrontBuffer) {
                    for (int x = 0; x < MAP_DATA_SIZE; ++x) {
                        for (int z = 0; z < MAP_DATA_SIZE; ++z) {
                            int sourceX = x + shiftX;
                            int sourceZ = z + shiftZ;
                            size_t index = static_cast<size_t>(x) * MAP_DATA_SIZE + z;
                            if (sourceX < 0 || sourceX >= MAP_DATA_SIZE || sourceZ < 0 || sourceZ >= MAP_DATA_SIZE) {
                                newEdgeCells[index] = 1;
                                continue;
                            }
                            g_mapColorsBack[x][z] = g_mapColors[sourceX][sourceZ];
                            g_mapHeightsBack[x][z] = g_mapHeights[sourceX][sourceZ];
                            g_mapWaterFlagsBack[x][z] = g_mapWaterFlags[sourceX][sourceZ];
                            g_mapBrightnessBack[x][z] = g_mapBrightness[sourceX][sourceZ];
                            g_mapChannelHeightsBack[x][z] = g_mapChannelHeights[sourceX][sourceZ];
                        }
                    }
                }
            }

            scanOrder.clear();
            scanOrder.reserve(MAP_DATA_SIZE * MAP_DATA_SIZE);
            std::vector<unsigned char> queued(MAP_DATA_SIZE * MAP_DATA_SIZE, 0);
            auto enqueueCell = [&](int dx, int dz) {
                if (dx < -MAP_DATA_RADIUS || dx > MAP_DATA_RADIUS || dz < -MAP_DATA_RADIUS || dz > MAP_DATA_RADIUS) return;
                int arrX = dx + MAP_DATA_RADIUS;
                int arrZ = dz + MAP_DATA_RADIUS;
                size_t index = static_cast<size_t>(arrX) * MAP_DATA_SIZE + arrZ;
                if (queued[index]) return;
                queued[index] = 1;
                scanOrder.push_back({static_cast<short>(dx), static_cast<short>(dz)});
            };

            // 小地图可见方形区域必须先完整扫描，再整体发布到前台贴图。
            for (int dx = -50; dx <= 50; ++dx) {
                for (int dz = -50; dz <= 50; ++dz) enqueueCell(dx, dz);
            }
            visibleScanCellCount = scanOrder.size();

            // 玩家移动后新出现的边缘不等待远区的低频刷新。
            if (canReuseFrontBuffer) {
                for (int x = 0; x < MAP_DATA_SIZE; ++x) {
                    for (int z = 0; z < MAP_DATA_SIZE; ++z) {
                        if (newEdgeCells[static_cast<size_t>(x) * MAP_DATA_SIZE + z]) {
                            enqueueCell(x - MAP_DATA_RADIUS, z - MAP_DATA_RADIUS);
                        }
                    }
                }
            }

            auto isUnloadedCell = [&](int dx, int dz) {
                int arrX = dx + MAP_DATA_RADIUS;
                int arrZ = dz + MAP_DATA_RADIUS;
                return prevCave ? g_mapBrightnessBack[arrX][arrZ] < 0.0f : g_mapColorsBack[arrX][arrZ].a <= 0.01f;
            };
            auto enqueueRing = [&](int radius, bool unloadedFirst) {
                auto enqueueIfMatched = [&](int dx, int dz) {
                    if (isUnloadedCell(dx, dz) == unloadedFirst) enqueueCell(dx, dz);
                };
                for (int dz = -radius; dz <= radius; ++dz) {
                    enqueueIfMatched(-radius, dz);
                    enqueueIfMatched(radius, dz);
                }
                for (int dx = -radius + 1; dx < radius; ++dx) {
                    enqueueIfMatched(dx, -radius);
                    enqueueIfMatched(dx, radius);
                }
            };
            // 每一圈先尝试未加载格，再刷新已有格；近圈总是先于远圈。
            for (int radius = 51; radius <= MAP_DATA_RADIUS; ++radius) {
                enqueueRing(radius, true);
                enqueueRing(radius, false);
            }
            std::fill(
                &caveChannelDrops[0][0],
                &caveChannelDrops[0][0] + MAP_DATA_SIZE * MAP_DATA_SIZE,
                static_cast<signed char>(-1)
            );
            std::memset(caveChunkLoadStates, -1, sizeof(caveChunkLoadStates));
            scanPhase = prevCave ? ScanPhase::CaveSeedVisible : ScanPhase::SurfaceVisible;
            scanCursor = 0;
            minimapSnapshotPublished = false;
            outerCellsSincePublish = 0;
            visibleAreaHadUnloadedChunks = false;
            caveChunkBaseX = (currentScanX - MAP_DATA_RADIUS) >> 4;
            caveChunkBaseZ = (currentScanZ - MAP_DATA_RADIUS) >> 4;
            caveLightRefreshActive = false;
            g_caveLightRefreshReady.store(false);
            isScanning = true;
        }

        if (isScanning) {
            try {
                BlockSource* regionPtr = clientInstance->getRegion();
                if (regionPtr) {
                    BlockSource& region = *regionPtr;
                    int rowsThisFrame = 0;

                    static std::string s_lastBlockName = "";
                    static mce::Color s_lastBlockColor(0, 0, 0, 0);
                    static int s_biomeCellX = -99999;
                    static int s_biomeCellZ = -99999;
                    static std::string s_biomeName = "";
                    static mce::Color s_cachedGrass(0,0,0,0), s_cachedFoliage(0,0,0,0), s_cachedWater(0,0,0,0);

                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    static std::hash<std::string> hasher;
                    auto isChunkLoaded = [&](int x, int z) {
                        int gridX = (x >> 4) - caveChunkBaseX;
                        int gridZ = (z >> 4) - caveChunkBaseZ;
                        if (gridX < 0 || gridX >= kCaveChunkGridSize || gridZ < 0 || gridZ >= kCaveChunkGridSize) {
                            return region.hasChunksAt(BlockPos(x, currentScanY, z), 0, false);
                        }
                        signed char& state = caveChunkLoadStates[gridX][gridZ];
                        if (state <= 0) {
                            if (region.hasChunksAt(BlockPos(x, currentScanY, z), 0, false)) {
                                state = 1;
                            } else {
                                state = 0;
                                return false;
                            }
                        }
                        return true;
                    };

                    auto isOuterPhase = [&]() {
                        return scanPhase == ScanPhase::SurfaceOuter || scanPhase == ScanPhase::CaveSeedOuter ||
                            scanPhase == ScanPhase::CaveResolveOuter;
                    };
                    auto isCaveSeedPhase = [&]() {
                        return scanPhase == ScanPhase::CaveSeedVisible || scanPhase == ScanPhase::CaveSeedOuter;
                    };
                    auto phaseEnd = [&]() {
                        return isOuterPhase() ? scanOrder.size() : visibleScanCellCount;
                    };

                    while (scanCursor < phaseEnd() && !timeBudgetExceeded) {
                        ScanCell const& scanCell = scanOrder[scanCursor];
                        int dx = scanCell.dx;
                        int arrX = dx + MAP_DATA_RADIUS;

                        {
                            int dz = scanCell.dz;
                            int targetX = currentScanX + dx;
                            int targetZ = currentScanZ + dz;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            if (isCaveSeedPhase()) {
                                // 全图从同一个 Top Y 向下找开放通道，水体和植被不会形成黑墙。
                                int channelDrop = -1;
                                if (isChunkLoaded(targetX, targetZ)) {
                                    try {
                                        for (int drop = 0; drop <= kCaveLayerAirSearchDepth; ++drop) {
                                            int candidateY = currentCaveTopY - drop;
                                            if (candidateY < -64) break;
                                            if (IsCavePassableBlock(region.getBlock(BlockPos(targetX, candidateY, targetZ)))) {
                                                channelDrop = drop;
                                                break;
                                            }
                                        }
                                    } catch (...) {
                                        channelDrop = -1;
                                    }
                                } else if (!isOuterPhase()) {
                                    visibleAreaHadUnloadedChunks = true;
                                }
                                caveChannelDrops[arrX][arrZ] = static_cast<signed char>(channelDrop);
                            } else if (prevCave) {
                                // 阶段二：只解析命中的开放列，从固定 Top Y 向下选择第一条可见通道。
                                mce::Color caveColor(0, 0, 0, 1);
                                int floorY = currentScanY;
                                int channelY = currentScanY;
                                float brightness = -1.0f;
                                bool isWaterCell = false;

                                int channelDrop = caveChannelDrops[arrX][arrZ];
                                if (channelDrop >= 0 && isChunkLoaded(targetX, targetZ)) {
                                    try {
                                        channelY = currentCaveTopY - channelDrop;
                                        bool foundChannel = true;

                                        if (foundChannel) {
                                            BlockPos channelPos(targetX, channelY, targetZ);
                                            isWaterCell = IsCaveWaterBlock(region.getBlock(channelPos));
                                            brightness = std::clamp(
                                                std::max(region.getBrightness(channelPos), region.getSkylightBrightness(channelPos) / 15.0f),
                                                0.0f,
                                                1.0f
                                            );

                                            bool foundFloor = false;
                                            for (int drop = 1; drop <= kCaveLayerFloorSearchDepth; ++drop) {
                                                int candidateY = channelY - drop;
                                                if (candidateY < -64) break;
                                                Block const& floorBlock = region.getBlock(BlockPos(targetX, candidateY, targetZ));
                                                isWaterCell = isWaterCell || IsCaveWaterBlock(floorBlock);
                                                if (IsCavePassableBlock(floorBlock)) continue;

                                                foundFloor = true;
                                                floorY = candidateY;
                                                std::string floorName = floorBlock.getTypeName();
                                                int cellX = targetX >> 2;
                                                int cellZ = targetZ >> 2;
                                                if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
                                                    s_biomeCellX = cellX;
                                                    s_biomeCellZ = cellZ;
                                                    try {
                                                        auto const& biome = region.getBiome(BlockPos(targetX, floorY, targetZ));
                                                        std::string newBiomeName = biome.mHash->getString();
                                                        if (s_biomeName != newBiomeName) {
                                                            s_biomeName = newBiomeName;
                                                            getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                        }
                                                    } catch (...) {
                                                        if (!s_biomeName.empty()) s_biomeName = "";
                                                    }
                                                }

                                                static std::unordered_map<size_t, mce::Color> s_caveColorCache;
                                                if (s_caveColorCache.size() > 20000) s_caveColorCache.clear();
                                                size_t cacheKey = hasher(floorName) ^ (hasher(s_biomeName) << 1);
                                                auto colorIt = s_caveColorCache.find(cacheKey);
                                                if (colorIt != s_caveColorCache.end()) {
                                                    caveColor = colorIt->second;
                                                } else {
                                                    caveColor = getBlockColor(floorName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                    s_caveColorCache[cacheKey] = caveColor;
                                                }
                                                break;
                                            }

                                            if (!foundFloor) {
                                                // 有空气但 64 格内没有地板，保留为低亮度深洞而不是泄露地表颜色。
                                                floorY = std::max(channelY - kCaveLayerFloorSearchDepth, -64);
                                                caveColor = mce::Color(0.08f, 0.08f, 0.08f, 1.0f);
                                            }

                                            if (isWaterCell) {
                                                caveColor = BlendWaterOverFloor(caveColor, s_cachedWater);
                                            }
                                        }
                                    } catch (...) {
                                        caveColor = mce::Color(0, 0, 0, 1);
                                        brightness = -1.0f;
                                    }
                                }

                                g_mapColorsBack[arrX][arrZ] = caveColor;
                                g_mapHeightsBack[arrX][arrZ] = (float)floorY;
                                g_mapWaterFlagsBack[arrX][arrZ] = isWaterCell;
                                g_mapBrightnessBack[arrX][arrZ] = brightness;
                                g_mapChannelHeightsBack[arrX][arrZ] = channelY;
                            } else if (!isChunkLoaded(targetX, targetZ)) {
                                if (!isOuterPhase()) visibleAreaHadUnloadedChunks = true;
                            } else {
                                // === 地表模式：现有扫描逻辑 ===
                                short topY = region.getAboveTopSolidBlock(targetX, targetZ, true, true);
                                g_mapHeightsBack[arrX][arrZ] = (float)topY;
                                bool isWaterCell = false;

                                if (topY > -64) {
                                    Block const& block = region.getBlock(BlockPos(targetX, topY - 1, targetZ));
                                    std::string blockName = block.getTypeName();
                                    bool hasSurfaceWater = IsCaveWaterBlock(block);

                                    if (blockName.find("snow") != std::string::npos) {
                                        g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                    } else {
                                        Block const& blockAbove = region.getBlock(BlockPos(targetX, topY, targetZ));
                                        std::string aboveName = blockAbove.getTypeName();

                                        if (aboveName == "minecraft:air" || aboveName == "air" ||
                                            aboveName.find("barrier") != std::string::npos ||
                                            aboveName.find("light_block") != std::string::npos ||
                                            aboveName.find("structure_void") != std::string::npos ||
                                            aboveName.find("placeholder") != std::string::npos ||
                                            aboveName.find("unknown") != std::string::npos ||
                                            aboveName.find("info_update") != std::string::npos) {
                                        } else if (aboveName.find("snow") != std::string::npos) {
                                            g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                            blockName = "";
                                        } else {
                                            blockName = aboveName;
                                        }

                                        if (!blockName.empty()) {
                                            int cellX = targetX >> 2;
                                            int cellZ = targetZ >> 2;
                                            if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
                                                s_biomeCellX = cellX; s_biomeCellZ = cellZ;
                                                try {
                                                    auto const& biome = region.getBiome(BlockPos(targetX, topY - 1, targetZ));
                                                    std::string newBiomeName = biome.mHash->getString();
                                                    if (s_biomeName != newBiomeName) {
                                                        s_biomeName = newBiomeName;
                                                        getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                    }
                                                } catch (...) {
                                                    if (!s_biomeName.empty()) { s_biomeName = ""; }
                                                }
                                            }

                                            static std::unordered_map<size_t, mce::Color> s_globalColorCache;
                                            if (s_globalColorCache.size() > 20000) s_globalColorCache.clear();

                                            size_t cacheKey = hasher(blockName) ^ (hasher(s_biomeName) << 1);
                                            auto it = s_globalColorCache.find(cacheKey);

                                            if (it != s_globalColorCache.end()) {
                                                g_mapColorsBack[arrX][arrZ] = it->second;
                                            } else {
                                                mce::Color calculatedColor = getBlockColor(blockName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                s_globalColorCache[cacheKey] = calculatedColor;
                                                g_mapColorsBack[arrX][arrZ] = calculatedColor;
                                            }
                                        }
                                    }

                                    // 水面列：以海床颜色作为底色，叠加水色；海床 Y 继续用于高度阴影。
                                    if (hasSurfaceWater) {
                                        isWaterCell = true;
                                        int seaFloor = (int)topY - 1;
                                        int surfaceY = seaFloor + 1;
                                        while (seaFloor > -64 && (surfaceY - seaFloor) < 64) {
                                            try {
                                                Block const& seaFloorBlock = region.getBlock(BlockPos(targetX, seaFloor, targetZ));
                                                if (!IsCavePassableBlock(seaFloorBlock)) break;
                                            } catch (...) { break; }
                                            seaFloor--;
                                        }
                                        g_mapHeightsBack[arrX][arrZ] = (float)seaFloor;
                                        try {
                                            Block const& seaFloorBlock = region.getBlock(BlockPos(targetX, seaFloor, targetZ));
                                            mce::Color seaFloorColor = getBlockColor(
                                                seaFloorBlock.getTypeName(),
                                                s_cachedGrass,
                                                s_cachedFoliage,
                                                s_cachedWater
                                            );
                                            g_mapColorsBack[arrX][arrZ] = BlendWaterOverFloor(seaFloorColor, s_cachedWater);
                                        } catch (...) {
                                            g_mapColorsBack[arrX][arrZ] = BlendWaterOverFloor(
                                                mce::Color(0.08f, 0.08f, 0.08f, 1.0f),
                                                s_cachedWater
                                            );
                                        }
                                    }
                                } else {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                                }
                                g_mapWaterFlagsBack[arrX][arrZ] = isWaterCell;
                            }

                            scanCursor++;
                            if (isOuterPhase() && !isCaveSeedPhase()) ++outerCellsSincePublish;

                            if ((scanCursor & 63) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                int budgetMicros = (prevCave || !isOuterPhase()) ? 6000 : 2500;
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > budgetMicros) {
                                    timeBudgetExceeded = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (minimapSnapshotPublished && outerCellsSincePublish >= 2048) {
                        publishScanBuffer();
                        outerCellsSincePublish = 0;
                    }

                    if (scanCursor >= phaseEnd()) {
                        switch (scanPhase) {
                        case ScanPhase::SurfaceVisible:
                            publishScanBuffer();
                            minimapSnapshotPublished = true;
                            hasPublishedScanBuffer = true;
                            publishedBufferWasCave = false;
                            scanPhase = ScanPhase::SurfaceOuter;
                            scanCursor = visibleScanCellCount;
                            break;
                        case ScanPhase::CaveSeedVisible:
                            scanPhase = ScanPhase::CaveResolveVisible;
                            scanCursor = 0;
                            break;
                        case ScanPhase::CaveResolveVisible:
                            publishScanBuffer();
                            minimapSnapshotPublished = true;
                            hasPublishedScanBuffer = true;
                            publishedBufferWasCave = true;
                            scanPhase = ScanPhase::CaveSeedOuter;
                            scanCursor = visibleScanCellCount;
                            break;
                        case ScanPhase::CaveSeedOuter:
                            scanPhase = ScanPhase::CaveResolveOuter;
                            scanCursor = visibleScanCellCount;
                            break;
                        case ScanPhase::SurfaceOuter:
                        case ScanPhase::CaveResolveOuter: {
                            publishScanBuffer();
                            outerCellsSincePublish = 0;
                            isScanning = false;
                            // 玩家可见范围存在尚未进入客户端的区块时，缩短下次尝试间隔。
                            ticksSinceScan = visibleAreaHadUnloadedChunks ? 80 : 0;

                            if (prevCave) {
                                caveLightRefreshActive = false;
                                caveLightRefreshIndex = 0;
                                caveLightRefreshCooldown = 0;
                                caveLightRefreshLeft = MAP_DATA_RADIUS - 50;
                                caveLightRefreshTop = MAP_DATA_RADIUS - 50;
                            } else {
                                using ColorGrid = mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
                                using HeightGrid = float[MAP_DATA_SIZE][MAP_DATA_SIZE];
                                using WaterGrid = bool[MAP_DATA_SIZE][MAP_DATA_SIZE];
                                auto asyncColors = new ColorGrid;
                                auto asyncHeights = new HeightGrid;
                                auto asyncWaterFlags = new WaterGrid;
                                std::memcpy(asyncColors, g_mapColorsBack, sizeof(g_mapColorsBack));
                                std::memcpy(asyncHeights, g_mapHeightsBack, sizeof(g_mapHeightsBack));
                                std::memcpy(asyncWaterFlags, g_mapWaterFlagsBack, sizeof(g_mapWaterFlagsBack));
                                int asyncX = currentScanX;
                                int asyncZ = currentScanZ;

                                std::thread([asyncX, asyncZ, asyncColors, asyncHeights, asyncWaterFlags]() {
                                    MapCacheManager::UpdateFromScan(asyncX, asyncZ, asyncColors, asyncHeights, asyncWaterFlags, false);
                                    delete[] asyncColors;
                                    delete[] asyncHeights;
                                    delete[] asyncWaterFlags;
                                }).detach();
                            }
                            break;
                        }
                        }
                    }
                }
            } catch (...) {}
        }

        // 完整洞穴扫描后，只在当前小地图可见的 101x101 区域轮询真实光照。
        // 每 tick 最多读取 512 格，整轮完成后才请求一次局部纹理重烘焙。
        if (isScanning || !prevCave || !MapRenderState::caveMode) {
            caveLightRefreshActive = false;
        } else {
            if (!caveLightRefreshActive && !g_caveLightRefreshReady.load()) {
                if (caveLightRefreshCooldown > 0) {
                    --caveLightRefreshCooldown;
                } else {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    int playerOffsetX = px - g_lastRenderX;
                    int playerOffsetZ = pz - g_lastRenderZ;
                    caveLightRefreshLeft = std::clamp(MAP_DATA_RADIUS + playerOffsetX - 50, 0, MAP_DATA_SIZE - 101);
                    caveLightRefreshTop = std::clamp(MAP_DATA_RADIUS + playerOffsetZ - 50, 0, MAP_DATA_SIZE - 101);
                    caveLightRefreshIndex = 0;
                    caveLightRefreshActive = true;
                }
            }

            if (caveLightRefreshActive) {
                struct CaveLightSample {
                    int mapX;
                    int mapZ;
                    int worldX;
                    int worldZ;
                    int channelY;
                };

                constexpr int kLightSamplesPerTick = 512;
                constexpr int kVisibleLightSize = 101;
                std::vector<CaveLightSample> samples;
                samples.reserve(kLightSamplesPerTick);
                int refreshCenterX = 0;
                int refreshCenterZ = 0;

                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    refreshCenterX = g_lastRenderX;
                    refreshCenterZ = g_lastRenderZ;
                    int samplesThisTick = std::min(kLightSamplesPerTick, kVisibleLightSize * kVisibleLightSize - caveLightRefreshIndex);
                    for (int i = 0; i < samplesThisTick; ++i) {
                        int sequence = caveLightRefreshIndex++;
                        int mapX = caveLightRefreshLeft + sequence % kVisibleLightSize;
                        int mapZ = caveLightRefreshTop + sequence / kVisibleLightSize;
                        if (g_mapBrightness[mapX][mapZ] < 0.0f) continue;
                        samples.push_back({
                            mapX,
                            mapZ,
                            refreshCenterX + mapX - MAP_DATA_RADIUS,
                            refreshCenterZ + mapZ - MAP_DATA_RADIUS,
                            g_mapChannelHeights[mapX][mapZ]
                        });
                    }
                }

                if (BlockSource* region = clientInstance->getRegion()) {
                    struct CaveLightResult {
                        int mapX;
                        int mapZ;
                        float brightness;
                    };
                    std::vector<CaveLightResult> results;
                    results.reserve(samples.size());
                    for (auto const& sample : samples) {
                        try {
                            BlockPos channelPos(sample.worldX, sample.channelY, sample.worldZ);
                            if (!region->hasChunksAt(channelPos, 0, false) ||
                                !IsCavePassableBlock(region->getBlock(channelPos))) continue;
                            float light = std::clamp(
                                std::max(region->getBrightness(channelPos), region->getSkylightBrightness(channelPos) / 15.0f),
                                0.0f,
                                1.0f
                            );
                            results.push_back({sample.mapX, sample.mapZ, light});
                        } catch (...) {}
                    }

                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    if (MapRenderState::caveMode &&
                        g_lastRenderX == refreshCenterX && g_lastRenderZ == refreshCenterZ) {
                        for (auto const& result : results) {
                            if (g_mapBrightness[result.mapX][result.mapZ] >= 0.0f) {
                                g_mapBrightness[result.mapX][result.mapZ] = result.brightness;
                            }
                        }
                    }
                }

                if (caveLightRefreshIndex >= kVisibleLightSize * kVisibleLightSize) {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    if (MapRenderState::caveMode &&
                        g_lastRenderX == refreshCenterX && g_lastRenderZ == refreshCenterZ) {
                        g_caveLightRefreshLeft = caveLightRefreshLeft;
                        g_caveLightRefreshTop = caveLightRefreshTop;
                        g_caveLightRefreshRight = caveLightRefreshLeft + kVisibleLightSize;
                        g_caveLightRefreshBottom = caveLightRefreshTop + kVisibleLightSize;
                        g_caveLightRefreshReady.store(true);
                    }
                    caveLightRefreshActive = false;
                    caveLightRefreshCooldown = 5;
                }
            }
        }

        static int entityDelay = 0;
        if (++entityDelay >= 15) {
            entityDelay = 0;
            std::vector<RadarEntity> tempEntities;
            auto& level = player->getLevel();
            const auto& entities = level.getRuntimeActorList();
            
            for (auto* actor : entities) {
                if (!actor || actor == player) continue;
                if (!actor->isAlive()) continue;

                const Vec3& ePos = actor->getPosition();
                float dx = ePos.x - pos.x;
                float dz = ePos.z - pos.z;

                if (dx * dx + dz * dz > MAP_DATA_RADIUS * MAP_DATA_RADIUS) continue;

                int type = 2;
                std::string entityType;
                std::string uuid;
                if (actor->isPlayer()) {
                    type = 0;
                    entityType = "player";
                    auto* p = static_cast<class Player*>(actor);
                    if (IsLocalRadarPlayer(*clientInstance, *player, *actor)) continue;
                    uuid = static_cast<std::string>(p->getUuid());
                    ExtractPlayerSkinHead(p, uuid);
                } else if (actor->hasCategory(ActorCategory::Item)) {
                    type = 3;
                    entityType = "item";
                } else if (actor->hasCategory(ActorCategory::Monster)) {
                    type = 1;
                    entityType = actor->getTypeName();
                } else {
                    type = 2;
                    entityType = actor->getTypeName();
                }

                tempEntities.push_back({ePos.x, ePos.y, ePos.z, type, entityType, uuid});
            }
            {
                std::lock_guard<std::mutex> lock(g_radarMutex);
                g_radarEntities = std::move(tempEntities);
            }
            g_radarGeneration.fetch_add(1, std::memory_order_release);
        }

    } else {
        g_hasPlayer   = false;
        g_localPlayer = nullptr;
        MapRenderState::showBigMap = false;
        MapRenderState::showWaypointUI = false;
        MapRenderState::showPositionSettings = false;
    }

}

LL_TYPE_INSTANCE_HOOK(
    ClientInstanceUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool a1
) {
    bool result = origin(a1);
    HandleClientInstanceUpdate(this, a1 != 0);
    return result;
}

// ==========================================
// [大地图模式] 视角与交互底层物理锁死模块
// ==========================================

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerApplyTurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    Vec2 const& rotationDelta
) {
    if (MapRenderState::IsUIActive()) return;
    origin(rotationDelta);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeStartDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$startDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    bool& isDestroyed
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, isDestroyed);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItem,
    bool,
    ItemStack& item
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(item);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerPickBlockHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::pickBlock,
    void,
    HitResult const& hitResult,
    bool withData
) {
    if (MapRenderState::IsUIActive()) return;
    origin(hitResult, withData);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySelectSlotHook,
    ll::memory::HookPriority::Normal,
    PlayerInventory,
    &PlayerInventory::selectSlot,
    bool,
    int slot,
    ContainerID containerId
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(slot, containerId);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySetItemHook,
    ll::memory::HookPriority::Normal,
    Inventory,
    &Inventory::$setItem,
    void,
    int slot,
    ItemStack const& item
) {
    if (MapRenderState::IsUIActive()) return;
    origin(slot, item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$attack,
    bool,
    Actor& actor
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(actor);
}

LL_TYPE_INSTANCE_HOOK(
    ActorIsImmobileHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::$isImmobile,
    bool
) {
    if (MapRenderState::showBigMap && g_localPlayer && this == (Actor*)g_localPlayer) return true;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSwingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$swing,
    bool,
    ActorSwingSource swingSource
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(swingSource);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemOnHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemOn,
    InteractionResult,
    ItemStack& item,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& hitPos,
    Block const* block,
    bool isFirstEvent
) {
    if (MapRenderState::IsUIActive()) return InteractionResult{false, false};
    return origin(item, pos, face, hitPos, block, isFirstEvent);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeContinueDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$continueDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& playerPos,
    bool& hasDestroyedBlock
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, playerPos, hasDestroyedBlock);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerJumpFromGroundHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::canJump,
    bool
) {
    if (MapRenderState::showBigMap && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return false;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSetSneakingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$setSneaking,
    void,
    bool isSneaking
) {
    if (MapRenderState::showBigMap && g_localPlayer && this == g_localPlayer) return;
    origin(isSneaking);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerIsImmobileHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$isImmobile,
    bool
) {
    if (MapRenderState::showBigMap && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return true;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    MobSetCarriedItemHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::$setCarriedItem,
    void,
    ItemStack const& item
) {
    if (MapRenderState::showBigMap && g_localPlayer && this == (Actor*)g_localPlayer) return;
    origin(item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeBaseUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::baseUseItem,
    bool,
    ItemStack const& item
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(item);
}
