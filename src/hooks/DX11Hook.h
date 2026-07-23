#pragma once
#include <windows.h>
#include <string>
// 定义这些宏，避免与 LeviLamina 中的重复定义冲突
#define D3D12_FEATURE_DATA_D3D12_OPTIONS  D3D12_FEATURE_DATA_D3D12_OPTIONS_LEGACY
#define D3D12_FEATURE_DATA_ARCHITECTURE  D3D12_FEATURE_DATA_ARCHITECTURE_LEGACY
#define D3D12_RAYTRACING_GEOMETRY_DESC  D3D12_RAYTRACING_GEOMETRY_DESC_LEGACY
// 包含我们需要的 DX 头文件
#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
// 取消这些宏定义，避免后续问题
#undef D3D12_FEATURE_DATA_D3D12_OPTIONS
#undef D3D12_FEATURE_DATA_ARCHITECTURE
#undef D3D12_RAYTRACING_GEOMETRY_DESC
// 现在包含其他头文件
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include <ll/api/memory/Hook.h>
#include <MinHook.h>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <windows.h>
#include "hooks/PlayerHook.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"
#include "state/DeathPointManager.h"
#include "state/ExternalCompassSync.h"

#include "state/LanguageManager.h"
#include <wincodec.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX11Hook {
    
    // 【极致平滑引擎】供全局调用的亚像素平滑坐标
    inline float g_smoothPX = 0.0f;
    inline float g_smoothPZ = 0.0f;

    inline void DbgLog(const char* msg, HRESULT hr = 0) {
        std::ofstream out("MapMod_Debug.txt", std::ios::app);
        if (hr != 0) {
            char hexBuf[32]; snprintf(hexBuf, sizeof(hexBuf), " (HR: 0x%08X)", hr);
            out << msg << hexBuf << "\n";
        } else {
            out << msg << "\n";
        }
    }

    enum class OreButtonKind { Default, Primary, Success, Warning, Danger, Compass };

    inline ImVec4 OreColor(int r, int g, int b, int a = 255) {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    inline void ApplyOreUIStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(14.0f, 12.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.CellPadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
        style.WindowRounding = 5.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = OreColor(236, 238, 240);
        colors[ImGuiCol_TextDisabled] = OreColor(135, 143, 150);
        colors[ImGuiCol_WindowBg] = OreColor(25, 27, 29, 238);
        colors[ImGuiCol_ChildBg] = OreColor(31, 34, 36, 230);
        colors[ImGuiCol_PopupBg] = OreColor(23, 25, 27, 248);
        colors[ImGuiCol_Border] = OreColor(86, 92, 98, 190);
        colors[ImGuiCol_FrameBg] = OreColor(45, 49, 52, 245);
        colors[ImGuiCol_FrameBgHovered] = OreColor(57, 62, 66, 255);
        colors[ImGuiCol_FrameBgActive] = OreColor(68, 74, 79, 255);
        colors[ImGuiCol_TitleBg] = OreColor(18, 20, 22, 255);
        colors[ImGuiCol_TitleBgActive] = OreColor(31, 34, 36, 255);
        colors[ImGuiCol_Button] = OreColor(58, 63, 67, 255);
        colors[ImGuiCol_ButtonHovered] = OreColor(74, 81, 86, 255);
        colors[ImGuiCol_ButtonActive] = OreColor(48, 53, 57, 255);
        colors[ImGuiCol_Header] = OreColor(59, 65, 69, 210);
        colors[ImGuiCol_HeaderHovered] = OreColor(73, 80, 86, 235);
        colors[ImGuiCol_HeaderActive] = OreColor(88, 97, 104, 255);
        colors[ImGuiCol_CheckMark] = OreColor(108, 214, 140);
        colors[ImGuiCol_SliderGrab] = OreColor(234, 197, 79);
        colors[ImGuiCol_SliderGrabActive] = OreColor(255, 222, 105);
        colors[ImGuiCol_Separator] = OreColor(89, 96, 102, 180);
        colors[ImGuiCol_ScrollbarBg] = OreColor(18, 20, 22, 180);
        colors[ImGuiCol_ScrollbarGrab] = OreColor(83, 91, 97, 240);
        colors[ImGuiCol_ScrollbarGrabHovered] = OreColor(104, 114, 122, 255);
        colors[ImGuiCol_ScrollbarGrabActive] = OreColor(123, 135, 144, 255);
    }

    inline void PushOreButtonStyle(OreButtonKind kind) {
        ImVec4 base = OreColor(58, 63, 67);
        ImVec4 hover = OreColor(74, 81, 86);
        ImVec4 active = OreColor(48, 53, 57);
        if (kind == OreButtonKind::Primary) { base = OreColor(40, 122, 173); hover = OreColor(54, 146, 199); active = OreColor(30, 96, 140); }
        else if (kind == OreButtonKind::Success) { base = OreColor(56, 139, 80); hover = OreColor(68, 166, 96); active = OreColor(43, 112, 64); }
        else if (kind == OreButtonKind::Warning) { base = OreColor(158, 118, 38); hover = OreColor(188, 143, 50); active = OreColor(130, 95, 28); }
        else if (kind == OreButtonKind::Danger) { base = OreColor(168, 58, 56); hover = OreColor(202, 72, 69); active = OreColor(132, 42, 41); }
        else if (kind == OreButtonKind::Compass) { base = OreColor(191, 126, 40); hover = OreColor(220, 151, 55); active = OreColor(154, 96, 28); }
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    }

    inline void PopOreButtonStyle() {
        ImGui::PopStyleColor(3);
    }

    inline void OreSectionHeader(const char* text) {
        ImGui::Spacing();
        ImGui::TextColored(OreColor(234, 197, 79), "%s", text);
        ImGui::Separator();
    }

    inline void OreTag(const char* text, ImVec4 color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("[%s]", text);
        ImGui::PopStyleColor();
    }

    inline ImVec4 ExternalCompassStatusColor() {
        switch (MapRenderState::externalCompassStatus.load()) {
        case 1:
        case 2: return OreColor(234, 197, 79);
        case 3: return OreColor(108, 214, 140);
        case 4: return OreColor(220, 92, 86);
        default: return OreColor(145, 153, 160);
        }
    }

    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    typedef void(__stdcall* ExecuteCommandLists_t)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    inline Present_t oPresent = nullptr;
    inline Present1_t oPresent1 = nullptr;
    inline ResizeBuffers_t oResizeBuffers = nullptr;
    inline ExecuteCommandLists_t oExecuteCommandLists = nullptr;

    inline ID3D11Device* g_pd3dDevice = nullptr;
    inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    inline ID3D11On12Device* g_d3d11On12Device = nullptr;
    inline ID3D12CommandQueue* g_pGameCommandQueue = nullptr; 
    
    inline HWND g_hWnd = nullptr;
    inline WNDPROC oWndProc = nullptr;
    inline bool g_imguiInitialized = false;

    inline ID3D11Texture2D* g_mapTexture = nullptr;
    inline ID3D11ShaderResourceView* g_mapTextureView = nullptr;
    inline uint8_t g_textureData[MAP_DATA_SIZE * MAP_DATA_SIZE * 4];

    inline std::unordered_map<uint64_t, ID3D11Texture2D*> g_regionTextures;
    inline std::unordered_map<uint64_t, ID3D11ShaderResourceView*> g_regionSRVs;

    // 记录上一次真实推送到 DX11 纹理的坐标中心
    inline float g_textureCenterX = 0.0f;
    inline float g_textureCenterZ = 0.0f;

    // BetterRenderDragon 兼容：检测是否加载，决定后缓冲区资源状态转换策略
    inline bool g_brdPresent = false;

    inline HWND FindMinecraftWindow() {
        HWND hwnd = FindWindowA("Bedrock", "Minecraft");
        if (!hwnd) hwnd = FindWindowW(L"Minecraft", NULL);
        if (!hwnd) hwnd = GetForegroundWindow();
        return hwnd;
    }

    inline const char* ExternalCompassStatusText() {
        switch (MapRenderState::externalCompassStatus.load()) {
        case 1: return "scanning";
        case 2: return "connecting";
        case 3: return "connected";
        case 4: return "retrying";
        default: return "disabled";
        }
    }

    // ==========================================
    // 生物头像系统
    // ==========================================
    inline const int FACE_SIZE = 12; // 面孔贴图尺寸 12x12

    // 面孔颜色方案
    struct MobFace {
        uint8_t skin[3];
        uint8_t eye[3];
        uint8_t eyePupil[3];
        uint8_t mouth[3];
        uint8_t hair[3];
        uint8_t accent[3];
    };

    // 已知生物类型 → 面孔颜色查找表
    inline std::unordered_map<std::string, DX11Hook::MobFace> g_typeToFace = {
        // 敌对生物
        {"minecraft:zombie",            {{80,130,50},{200,200,200},{0,0,0},{60,30,10},{50,70,30},{0,0,0}}},
        {"minecraft:skeleton",          {{190,190,190},{0,0,0},{0,0,0},{80,80,80},{190,190,190},{80,80,80}}},
        {"minecraft:creeper",           {{100,180,100},{255,255,255},{0,0,0},{0,0,0},{100,180,100},{0,0,0}}},
        {"minecraft:spider",            {{80,50,40},{200,50,50},{0,0,0},{30,20,15},{50,30,20},{0,0,0}}},
        {"minecraft:cave_spider",       {{70,100,120},{200,50,50},{0,0,0},{30,40,50},{50,70,90},{0,0,0}}},
        {"minecraft:enderman",          {{30,20,40},{200,50,200},{0,0,0},{10,10,20},{10,5,20},{200,50,200}}},
        {"minecraft:blaze",             {{220,180,50},{255,200,50},{0,0,0},{180,100,20},{200,150,30},{0,0,0}}},
        {"minecraft:ghast",             {{220,220,220},{200,50,50},{0,0,0},{150,150,150},{220,220,220},{0,0,0}}},
        {"minecraft:slime",             {{80,200,80},{255,255,255},{0,0,0},{40,100,40},{60,150,60},{0,0,0}}},
        {"minecraft:magma_cube",        {{200,120,40},{255,200,50},{0,0,0},{150,80,20},{180,100,30},{0,0,0}}},
        {"minecraft:witch",             {{80,60,50},{200,50,50},{0,0,0},{50,30,20},{20,20,30},{100,100,30}}},
        {"minecraft:guardian",          {{150,180,200},{255,255,200},{0,0,0},{100,120,140},{80,120,150},{200,200,0}}},
        {"minecraft:elder_guardian",    {{120,150,180},{255,255,200},{0,0,0},{80,100,120},{60,100,130},{200,200,0}}},
        {"minecraft:silverfish",        {{120,120,120},{0,0,0},{0,0,0},{80,80,80},{100,100,100},{0,0,0}}},
        {"minecraft:endermite",         {{60,40,50},{200,50,200},{0,0,0},{40,25,35},{40,25,35},{0,0,0}}},
        {"minecraft:shulker",           {{140,100,160},{200,200,200},{0,0,0},{100,70,120},{120,80,140},{0,0,0}}},
        {"minecraft:phantom",           {{60,80,100},{200,50,50},{0,0,0},{40,50,70},{40,60,80},{0,0,0}}},
        {"minecraft:drowned",           {{60,100,80},{150,200,150},{0,0,0},{30,60,40},{30,80,50},{0,150,150}}},
        {"minecraft:husk",              {{150,140,100},{200,200,150},{0,0,0},{100,90,60},{120,110,70},{0,0,0}}},
        {"minecraft:stray",             {{160,170,180},{200,100,200},{0,0,0},{100,110,120},{120,130,140},{150,150,255}}},
        {"minecraft:wither_skeleton",   {{40,40,40},{200,50,50},{0,0,0},{20,20,20},{20,20,20},{0,0,0}}},
        {"minecraft:zombie_villager",   {{80,130,50},{200,200,200},{0,0,0},{60,30,10},{50,70,30},{150,100,50}}},
        {"minecraft:zombified_piglin",  {{120,100,80},{200,50,50},{0,0,0},{80,60,40},{100,80,60},{255,150,50}}},
        {"minecraft:piglin",            {{200,140,100},{200,100,50},{0,0,0},{150,100,60},{180,120,80},{255,150,50}}},
        {"minecraft:piglin_brute",      {{200,130,90},{200,50,50},{0,0,0},{150,90,50},{180,110,70},{200,100,0}}},
        {"minecraft:hoglin",            {{150,100,80},{200,150,100},{0,0,0},{100,60,40},{120,80,60},{100,60,40}}},
        {"minecraft:ravager",           {{80,80,80},{200,50,50},{0,0,0},{50,50,50},{60,60,60},{0,0,0}}},
        {"minecraft:wither",            {{30,30,30},{200,50,50},{0,0,0},{15,15,15},{20,20,20},{0,0,0}}},
        {"minecraft:ender_dragon",      {{30,20,40},{200,50,200},{0,0,0},{10,10,20},{10,5,20},{150,0,200}}},
        {"minecraft:zoglin",            {{90,110,70},{200,50,50},{0,0,0},{60,80,40},{70,90,50},{100,60,40}}},
        {"minecraft:vex",               {{160,160,180},{200,50,50},{0,0,0},{120,120,140},{140,140,160},{0,0,0}}},
        {"minecraft:evoker",            {{140,120,100},{200,200,200},{0,0,0},{100,80,60},{120,100,80},{150,150,150}}},
        {"minecraft:vindicator",        {{140,120,100},{200,200,200},{0,0,0},{100,80,60},{120,100,80},{0,0,0}}},
        {"minecraft:pillager",          {{140,120,100},{200,200,200},{0,0,0},{100,80,60},{120,100,80},{80,80,80}}},
        {"minecraft:warden",            {{20,40,80},{200,200,255},{0,0,0},{10,20,50},{10,20,50},{0,200,255}}},
        {"minecraft:bogged",            {{100,120,80},{200,50,200},{0,0,0},{60,80,40},{80,100,60},{0,0,0}}},
        {"minecraft:breeze",            {{150,180,200},{255,255,255},{0,0,0},{100,130,150},{120,150,170},{200,220,255}}},
        {"minecraft:creaking",          {{100,80,60},{200,200,100},{0,0,0},{60,40,20},{80,60,40},{0,0,0}}},
        {"minecraft:happy_ghast",       {{255,200,200},{255,255,255},{0,0,0},{100,50,50},{255,150,150},{255,100,100}}},
        {"minecraft:illusioner",        {{140,120,100},{200,200,200},{0,0,0},{100,80,60},{120,100,80},{100,200,100}}},
        {"minecraft:armor_stand",       {{160,140,120},{0,0,0},{0,0,0},{100,90,80},{130,115,100},{0,0,0}}},
        // 友好/中立生物
        {"minecraft:cow",               {{150,100,80},{255,255,255},{0,0,0},{80,50,30},{120,80,60},{200,200,200}}},
        {"minecraft:pig",               {{200,150,150},{255,255,255},{0,0,0},{150,100,100},{180,130,130},{0,0,0}}},
        {"minecraft:sheep",             {{200,200,200},{255,255,255},{0,0,0},{150,150,150},{180,180,180},{0,0,0}}},
        {"minecraft:chicken",           {{220,200,180},{255,150,50},{0,0,0},{180,160,140},{200,180,160},{200,50,50}}},
        {"minecraft:rabbit",            {{180,150,140},{255,255,255},{0,0,0},{130,100,90},{150,120,110},{0,0,0}}},
        {"minecraft:wolf",              {{150,150,150},{255,255,255},{0,0,0},{100,100,100},{130,130,130},{0,0,0}}},
        {"minecraft:fox",               {{200,120,60},{255,255,255},{0,0,0},{150,80,30},{180,100,40},{255,255,255}}},
        {"minecraft:cat",               {{200,150,100},{100,200,100},{0,0,0},{150,100,60},{180,130,80},{0,0,0}}},
        {"minecraft:ocelot",            {{200,180,100},{100,200,100},{0,0,0},{150,130,60},{180,160,80},{0,0,0}}},
        {"minecraft:horse",             {{120,80,60},{255,255,255},{0,0,0},{80,50,30},{100,60,40},{0,0,0}}},
        {"minecraft:donkey",            {{100,100,100},{255,255,255},{0,0,0},{60,60,60},{80,80,80},{0,0,0}}},
        {"minecraft:mule",              {{120,100,80},{255,255,255},{0,0,0},{80,60,40},{100,80,60},{0,0,0}}},
        {"minecraft:skeleton_horse",    {{150,160,170},{200,50,200},{0,0,0},{100,110,120},{130,140,150},{0,0,0}}},
        {"minecraft:zombie_horse",      {{80,120,60},{200,50,50},{0,0,0},{50,80,30},{60,100,40},{0,0,0}}},
        {"minecraft:bee",               {{220,180,50},{255,255,255},{0,0,0},{180,140,20},{200,160,30},{50,50,50}}},
        {"minecraft:dolphin",           {{100,140,180},{255,255,255},{0,0,0},{60,100,140},{80,120,160},{0,0,0}}},
        {"minecraft:squid",             {{100,120,150},{255,255,255},{0,0,0},{60,80,110},{80,100,130},{0,0,0}}},
        {"minecraft:glow_squid",        {{80,180,200},{0,200,200},{0,0,0},{40,140,160},{60,160,180},{0,255,200}}},
        {"minecraft:turtle",            {{80,140,80},{255,255,255},{0,0,0},{40,100,40},{60,120,60},{0,0,0}}},
        {"minecraft:panda",             {{200,200,200},{255,255,255},{0,0,0},{150,150,150},{180,180,180},{50,50,50}}},
        {"minecraft:polar_bear",        {{220,210,190},{255,255,255},{0,0,0},{180,170,150},{200,190,170},{0,0,0}}},
        {"minecraft:goat",              {{180,170,160},{255,200,100},{0,0,0},{140,130,120},{160,150,140},{0,0,0}}},
        {"minecraft:frog",              {{80,180,80},{255,255,200},{0,0,0},{40,140,40},{60,160,60},{0,0,0}}},
        {"minecraft:tadpole",           {{60,80,60},{0,0,0},{0,0,0},{30,50,30},{40,60,40},{0,0,0}}},
        {"minecraft:parrot",            {{50,180,50},{255,255,255},{0,0,0},{30,140,30},{40,160,40},{200,200,50}}},
        {"minecraft:llama",             {{180,160,130},{255,255,255},{0,0,0},{140,120,90},{160,140,110},{0,0,0}}},
        {"minecraft:trader_llama",      {{160,140,110},{255,255,255},{0,0,0},{120,100,70},{140,120,90},{100,200,100}}},
        {"minecraft:wandering_trader",  {{150,120,100},{255,255,255},{0,0,0},{110,80,60},{130,100,80},{0,150,200}}},
        {"minecraft:strider",           {{200,100,100},{255,200,200},{0,0,0},{150,60,60},{180,80,80},{0,0,0}}},
        {"minecraft:iron_golem",        {{150,120,100},{255,255,255},{0,0,0},{100,80,60},{120,100,80},{80,80,100}}},
        {"minecraft:snow_golem",        {{220,220,240},{255,255,255},{0,0,0},{180,180,200},{200,200,220},{255,150,50}}},
        {"minecraft:villager",          {{140,110,80},{255,255,255},{0,0,0},{100,70,40},{120,90,60},{150,100,50}}},
        {"minecraft:villager_v2",       {{140,110,80},{255,255,255},{0,0,0},{100,70,40},{120,90,60},{150,100,50}}},
        {"villager",                    {{140,110,80},{255,255,255},{0,0,0},{100,70,40},{120,90,60},{150,100,50}}},
        {"minecraft:mooshroom",         {{200,100,100},{255,255,255},{0,0,0},{150,60,60},{180,80,80},{200,50,50}}},
        {"minecraft:sniffer",           {{80,120,160},{255,255,200},{0,0,0},{40,80,120},{60,100,140},{0,0,0}}},
        {"minecraft:camel",             {{180,160,130},{255,255,255},{0,0,0},{140,120,90},{160,140,110},{100,80,50}}},
        {"minecraft:armadillo",         {{150,120,80},{255,255,255},{0,0,0},{110,80,40},{130,100,60},{0,0,0}}},
        {"minecraft:allay",             {{150,180,220},{255,255,200},{0,0,0},{100,130,170},{120,150,190},{200,200,255}}},
        {"minecraft:axolotl",          {{200,120,180},{255,200,200},{0,0,0},{150,80,130},{180,100,150},{0,0,0}}},
        {"minecraft:bat",              {{80,60,40},{255,255,255},{0,0,0},{50,30,20},{60,40,30},{0,0,0}}},
        {"minecraft:pufferfish",       {{200,200,100},{255,255,255},{0,0,0},{150,150,50},{180,180,80},{0,0,0}}},
        {"minecraft:tropical_fish",    {{255,150,50},{0,0,0},{255,255,255},{200,80,0},{255,200,100},{100,200,255}}},
        // 玩家
        {"player",                      {{180,140,100},{255,255,255},{80,60,40},{120,80,60},{100,70,40},{0,0,0}}},
    };

    // 生物类型→头像文件名映射（用于 WIC 加载 PNG）
    inline std::unordered_map<std::string, std::string> g_typeToFaceFile = {
        // 文件名规则: {Name}Face.png，部分特殊处理
        {"minecraft:zombie",            "ZombieFace.png"},
        {"minecraft:skeleton",          "SkeletonFace.png"},
        {"minecraft:creeper",           "CreeperFace.png"},
        {"minecraft:spider",            "SpiderFace.png"},
        {"minecraft:cave_spider",       "CaveSpiderFace.png"},
        {"minecraft:enderman",          "EndermanFace.png"},
        {"minecraft:blaze",             "BlazeFace.png"},
        {"minecraft:ghast",             "GhastFace.png"},
        {"minecraft:slime",             "SlimeFace.png"},
        {"minecraft:magma_cube",        "MagmaCubeFace.png"},
        {"minecraft:witch",             "WitchFace.png"},
        {"minecraft:guardian",          "GuardianFace.png"},
        {"minecraft:elder_guardian",    "ElderGuardianFace.png"},
        {"minecraft:silverfish",        "SilverfishFace.png"},
        {"minecraft:endermite",         "EndermiteFace.png"},
        {"minecraft:shulker",           "ShulkerFace.png"},
        {"minecraft:phantom",           "PhantomFace.png"},
        {"minecraft:drowned",           "DrownedFace.png"},
        {"minecraft:husk",              "HuskFace.png"},
        {"minecraft:stray",             "StrayFace.png"},
        {"minecraft:wither_skeleton",   "WitherSkeletonFace.png"},
        {"minecraft:zombie_villager",   "ZombieVillagerFace.png"},
        {"minecraft:zombified_piglin",  "ZombiePigmanFace.png"},
        {"minecraft:piglin",            "PiglinFace.png"},
        {"minecraft:piglin_brute",      "PiglinBruteFace.png"},
        {"minecraft:hoglin",            "HoglinFace.png"},
        {"minecraft:zoglin",            "ZoglinFace.png"},
        {"minecraft:ravager",           "RavagerFace.png"},
        {"minecraft:wither",            "WitherFace.png"},
        {"minecraft:ender_dragon",      "EnderdragonFace.png"},
        {"minecraft:vex",               "VexFace.png"},
        {"minecraft:evoker",            "EvokerFace.png"},
        {"minecraft:vindicator",        "VindicatorFace.png"},
        {"minecraft:pillager",          "PillagerFace.png"},
        {"minecraft:warden",            "WardenFace.png"},
        {"minecraft:bogged",            "BoggedPotatoFace.png"},
        {"minecraft:breeze",            "BreezeFace.png"},
        {"minecraft:happy_ghast",       "HappyGhastFace.png"},
        {"minecraft:creaking",          "CreakingFace.png"},
        {"minecraft:illusioner",        "IllusionerFace.png"},
        {"minecraft:cow",               "CowFace.png"},
        {"minecraft:pig",               "PigFace.png"},
        {"minecraft:sheep",             "SheepFace.png"},
        {"minecraft:chicken",           "ChickenFace.png"},
        {"minecraft:rabbit",            "RabbitFace.png"},
        {"minecraft:wolf",              "WolfFace.png"},
        {"minecraft:fox",               "FoxFace.png"},
        {"minecraft:cat",               "CatFace.png"},
        {"minecraft:ocelot",            "OcelotFace.png"},
        {"minecraft:horse",             "HorseFace.png"},
        {"minecraft:donkey",            "DonkeyFace.png"},
        {"minecraft:mule",              "MuleFace.png"},
        {"minecraft:skeleton_horse",    "SkeletonHorseFace.png"},
        {"minecraft:zombie_horse",      "ZombieHorseFace.png"},
        {"minecraft:bee",               "BeeFace.png"},
        {"minecraft:dolphin",           "DolphinFace.png"},
        {"minecraft:squid",             "SquidFace.png"},
        {"minecraft:glow_squid",        "GlowSquidFace.png"},
        {"minecraft:turtle",            "TurtleFace.png"},
        {"minecraft:panda",             "PandaFace.png"},
        {"minecraft:polar_bear",        "PolarBearFace.png"},
        {"minecraft:goat",              "GoatFace.png"},
        {"minecraft:frog",              "FrogFace.png"},
        {"minecraft:tadpole",           "TadpoleFace.png"},
        {"minecraft:parrot",            "ParrotFace.png"},
        {"minecraft:llama",             "LlamaFace.png"},
        {"minecraft:trader_llama",      "TraderLlamaFace.png"},
        {"minecraft:wandering_trader",  "WanderingTraderFace.png"},
        {"minecraft:strider",           "StriderFace.png"},
        {"minecraft:iron_golem",        "IronGolemFace.png"},
        {"minecraft:snow_golem",        "SnowGolemFace.png"},
        {"minecraft:villager",          "VillagerFace.png"},
        {"minecraft:villager_v2",       "VillagerFace.png"},
        {"villager",                    "VillagerFace.png"},
        {"minecraft:mooshroom",         "MooshroomFace.png"},
        {"minecraft:sniffer",           "SnifferFace.png"},
        {"minecraft:camel",             "CamelFace.png"},
        {"minecraft:armadillo",         "ArmadilloFace.png"},
        {"minecraft:armor_stand",       "ArmorStandFace.png"},
        {"minecraft:allay",             "AllayFace.png"},
        {"minecraft:axolotl",           "AxolotlFace.png"},
        {"minecraft:bat",               "BatFace.png"},
        {"minecraft:pufferfish",        "PufferfishFace.png"},
        // 新增生物头像（从 D:\map\resources 补充）
        {"minecraft:armorer",           "ArmorerFace.png"},
        {"minecraft:barnacle",          "BarnacleFace.png"},
        {"minecraft:butcher",           "ButcherFace.png"},
        {"minecraft:cartographer",      "CartographerFace.png"},
        {"minecraft:cleric",            "ClericFace.png"},
        {"minecraft:cluckshroom",       "CluckshroomFace.png"},
        {"minecraft:copper_golem",      "CopperGolemFace.png"},
        {"minecraft:farmer",            "FarmerFace.png"},
        {"minecraft:firefly",           "FireflyFace.png"},
        {"minecraft:fisherman",         "FishermanFace.png"},
        {"minecraft:fletcher",          "FletcherFace.png"},
        {"minecraft:herobrine",         "HerobrineFace.png"},
        {"minecraft:iceologer",         "IceologerFace.png"},
        {"minecraft:leatherworker",     "LeatherworkerFace.png"},
        {"minecraft:librarian",         "LibrarianFace.png"},
        {"minecraft:moobloom",          "MoobloomFace.png"},
        {"minecraft:moolip",            "MoolipFace.png"},
        {"minecraft:ostrich",           "OstrichFace.png"},
        {"minecraft:rascal",            "RascalFace.png"},
        {"minecraft:shepherd",          "ShepherdFace.png"},
        {"minecraft:sniffer",           "SnifferFace.png"},
        {"minecraft:vulture",           "VultureFace.png"},
        // 鱼（使用 Body.png）
        {"minecraft:cod",               "CodBody.png"},
        {"minecraft:salmon",            "SalmonBody.png"},
        {"minecraft:tropical_fish",     "TropicalFishBody.png"},
        // 玩家
        {"player",                      "SteveFace.png"},
    };

    // 贴图缓存
    inline std::unordered_map<std::string, ID3D11ShaderResourceView*> g_headTextures;
    inline std::unordered_map<std::string, ID3D11ShaderResourceView*> g_playerHeadTextures;
    inline bool g_tabHeld = false;

    // 程序化生成 12x12 RGBA 面孔贴图
    inline ID3D11ShaderResourceView* CreateProgrammaticFaceTexture(const MobFace& face) {
        const int S = FACE_SIZE;
        uint8_t pixels[S * S * 4];
        // 面孔像素模板 (S=12)
        // 索引映射: 0=hair,1=skin,2=eye,3=pupil,4=mouth,5=accent
        // 颜色来源数组
        const uint8_t* cols[6] = {face.hair, face.skin, face.eye, face.eyePupil, face.mouth, face.accent};
        // 12x12 模板: 每个 cell 是 cols 索引
        const int tmpl[12][12] = {
            {0,0,0,0,0,0,0,0,0,0,0,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,1,1,2,2,1,1,2,2,1,1,0},
            {0,1,1,3,3,1,1,3,3,1,1,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,1,1,1,1,4,4,1,1,1,1,0},
            {0,1,1,1,4,4,4,4,1,1,1,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,1,1,1,1,1,1,1,1,1,1,0},
            {0,0,0,0,0,0,0,0,0,0,0,0},
        };
        for (int y = 0; y < S; y++) {
            for (int x = 0; x < S; x++) {
                int ci = tmpl[y][x];
                int di = (y * S + x) * 4;
                pixels[di+0] = cols[ci][0];
                pixels[di+1] = cols[ci][1];
                pixels[di+2] = cols[ci][2];
                pixels[di+3] = 255;
            }
        }
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = S; desc.Height = S;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = pixels; init.SysMemPitch = S * 4;
        ID3D11Texture2D* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &init, &tex))) {
            g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
            tex->Release();
        }
        return srv;
    }

    // WIC 加载 PNG 文件为 D3D11 贴图
    inline ID3D11ShaderResourceView* LoadPNGAsTexture(const std::string& filePath) {
        HRESULT hr;
        IWICImagingFactory* factory = nullptr;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) return nullptr;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wpath[0], wlen);
        IWICBitmapDecoder* decoder = nullptr;
        hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr)) { factory->Release(); return nullptr; }
        IWICBitmapFrameDecode* frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) { decoder->Release(); factory->Release(); return nullptr; }
        IWICFormatConverter* converter = nullptr;
        bool converterReady = false;
        if (SUCCEEDED(factory->CreateFormatConverter(&converter))) {
            converterReady = SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));
        }
        UINT w, h;
        frame->GetSize(&w, &h);
        if (w == 0 || h == 0) { converterReady = false; }
        ID3D11ShaderResourceView* srv = nullptr;
        if (converterReady) {
            std::vector<uint8_t> pixels(w * h * 4);
            if (SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data()))) {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = w; desc.Height = h;
                desc.MipLevels = 1; desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA init = {};
                init.pSysMem = pixels.data(); init.SysMemPitch = w * 4;
                ID3D11Texture2D* tex = nullptr;
                if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &init, &tex))) {
                    g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
                    tex->Release();
                }
            }
        }
        if (converter) converter->Release();
        frame->Release(); decoder->Release(); factory->Release();
        return srv;
    }

    // 获取 DLL 所在目录（含尾部反斜杠）
    inline std::string GetModDir() {
        static std::string dir = []() {
            HMODULE hm = GetModuleHandleA("ChiyanMap.dll");
            if (!hm) return std::string("mods/ChiyanMap/");
            char buf[MAX_PATH];
            DWORD len = GetModuleFileNameA(hm, buf, MAX_PATH);
            if (len == 0) return std::string("mods/ChiyanMap/");
            std::string path(buf, len);
            auto pos = path.find_last_of("\\/");
            if (pos != std::string::npos) path = path.substr(0, pos + 1);
            return path;
        }();
        return dir;
    }

    // 获取生物头像 SRV（优先从 PNG 加载，失败则程序化生成）
    inline ID3D11ShaderResourceView* GetOrCreateFaceTexture(const std::string& typeName) {
        auto it = g_headTextures.find(typeName);
        if (it != g_headTextures.end()) return it->second;

        ID3D11ShaderResourceView* srv = nullptr;
        // 尝试从 PNG 加载
        auto fileIt = g_typeToFaceFile.find(typeName);
        if (fileIt != g_typeToFaceFile.end()) {
            std::string path = GetModDir() + "heads/" + fileIt->second;
            srv = LoadPNGAsTexture(path);
        }
        // 如果开头带 minecraft:，尝试去掉前缀再查
        if (!srv && typeName.size() > 10 && typeName.substr(0, 10) == "minecraft:") {
            std::string shortName = typeName.substr(10);
            auto fileIt2 = g_typeToFaceFile.find(shortName);
            if (fileIt2 != g_typeToFaceFile.end()) {
                std::string path = GetModDir() + "heads/" + fileIt2->second;
                srv = LoadPNGAsTexture(path);
            }
        }
        // 如果没有 minecraft: 前缀，尝试加上再查
        if (!srv) {
            std::string prefixed = "minecraft:" + typeName;
            auto fileIt2 = g_typeToFaceFile.find(prefixed);
            if (fileIt2 != g_typeToFaceFile.end()) {
                std::string path = GetModDir() + "heads/" + fileIt2->second;
                srv = LoadPNGAsTexture(path);
            }
        }
        // 失败则程序化生成
        if (!srv) {
            auto faceIt = g_typeToFace.find(typeName);
            if (faceIt == g_typeToFace.end() && typeName.size() > 10 && typeName.substr(0, 10) == "minecraft:") {
                faceIt = g_typeToFace.find(typeName.substr(10));
            }
            if (faceIt == g_typeToFace.end()) {
                faceIt = g_typeToFace.find("minecraft:" + typeName);
            }
            if (faceIt != g_typeToFace.end()) {
                srv = CreateProgrammaticFaceTexture(faceIt->second);
            }
        }
        g_headTextures[typeName] = srv;
        return srv;
    }

    // 获取玩家皮肤头部贴图（从缓存的 8x8 RGB 像素创建）
    inline ID3D11ShaderResourceView* GetOrCreatePlayerHeadTexture(const std::string& uuid) {
        auto it = g_playerHeadTextures.find(uuid);
        if (it != g_playerHeadTextures.end()) return it->second;

        PlayerSkinHead head;
        {
            std::lock_guard<std::mutex> lock(g_playerSkinMutex);
            auto hit = g_playerSkinHeads.find(uuid);
            if (hit == g_playerSkinHeads.end() || !hit->second.valid) return GetOrCreateFaceTexture("player");
            head = hit->second;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        // 将 8x8 放大到 FACE_SIZE x FACE_SIZE (最近邻采样)
        std::vector<uint8_t> pixels(FACE_SIZE * FACE_SIZE * 4);
        float scale = (float)FACE_SIZE / 8.0f;
        for (int y = 0; y < FACE_SIZE; y++) {
            for (int x = 0; x < FACE_SIZE; x++) {
                int sx = (int)(x / scale);
                int sy = (int)(y / scale);
                if (sx > 7) sx = 7; if (sy > 7) sy = 7;
                int si = (sy * 8 + sx) * 4;
                int di = (y * FACE_SIZE + x) * 4;
                pixels[di+0] = head.pixels[si+0];
                pixels[di+1] = head.pixels[si+1];
                pixels[di+2] = head.pixels[si+2];
                pixels[di+3] = head.pixels[si+3];
            }
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = FACE_SIZE; desc.Height = FACE_SIZE;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = pixels.data(); init.SysMemPitch = FACE_SIZE * 4;
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &init, &tex))) {
            g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
            tex->Release();
        }
        g_playerHeadTextures[uuid] = srv;
        return srv;
    }

    // ==========================================
    // [另辟蹊径] Windows Raw Input 硬件级欺骗器
    // ==========================================
    typedef UINT(WINAPI* PGETRAWINPUTDATA_HOOK)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
    inline PGETRAWINPUTDATA_HOOK oGetRawInputData = nullptr;

    inline UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
        if (MapRenderState::IsUIActive()) {
            return (UINT)-1;
        }
        if (oGetRawInputData) return oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
        return 0;
    }

    // ==========================================
    // [终极防御] 拦截 GetRawInputBuffer 与异步按键状态 (杀灭侧键)
    // ==========================================
    typedef UINT(WINAPI* PGETRAWINPUTBUFFER_HOOK)(PRAWINPUT, PUINT, UINT);
    inline PGETRAWINPUTBUFFER_HOOK oGetRawInputBuffer = nullptr;
    inline UINT WINAPI hkGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader) {
        if (MapRenderState::IsUIActive()) return (UINT)-1;
        if (oGetRawInputBuffer) return oGetRawInputBuffer(pData, pcbSize, cbSizeHeader);
        return 0;
    }

    typedef SHORT(WINAPI* PGETASYNCKEYSTATE_HOOK)(int);
    inline PGETASYNCKEYSTATE_HOOK oGetAsyncKeyState = nullptr;
    inline SHORT WINAPI hkGetAsyncKeyState(int vKey) {
        if (MapRenderState::IsUIActive() && vKey != VK_F11) {
            return 0;
        }
        if (oGetAsyncKeyState) return oGetAsyncKeyState(vKey);
        return 0;
    }

    typedef SHORT(WINAPI* PGETKEYSTATE_HOOK)(int);
    inline PGETKEYSTATE_HOOK oGetKeyState = nullptr;
    inline SHORT WINAPI hkGetKeyState(int vKey) {
        if (MapRenderState::IsUIActive() && vKey != VK_F11) {
            return 0;
        }
        if (oGetKeyState) return oGetKeyState(vKey);
        return 0;
    }

    typedef BOOL(WINAPI* PGETCURSORPOS_HOOK)(LPPOINT);
    inline PGETCURSORPOS_HOOK oGetCursorPos = nullptr;
    inline BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint) {
        if (oGetCursorPos) return oGetCursorPos(lpPoint);
        return FALSE;
    }

    typedef BOOL(WINAPI* PSETCURSORPOS_HOOK)(int, int);
    inline PSETCURSORPOS_HOOK oSetCursorPos = nullptr;
    inline BOOL WINAPI hkSetCursorPos(int X, int Y) {
        if (MapRenderState::IsUIActive()) return TRUE;
        if (oSetCursorPos) return oSetCursorPos(X, Y);
        return FALSE;
    }

    inline void ShutdownImGuiAndBuffers() {
        for(auto& p : g_regionSRVs) if(p.second) p.second->Release();
        g_regionSRVs.clear();
        for(auto& p : g_regionTextures) if(p.second) p.second->Release();
        g_regionTextures.clear();
        for(auto& p : g_headTextures) if(p.second) p.second->Release();
        g_headTextures.clear();
        for(auto& p : g_playerHeadTextures) if(p.second) p.second->Release();
        g_playerHeadTextures.clear();
        {
            std::lock_guard<std::mutex> lock(g_playerSkinMutex);
            g_playerSkinHeads.clear();
        }
        if (g_imguiInitialized) {
            if (g_mapTextureView) { g_mapTextureView->Release(); g_mapTextureView = nullptr; }
            if (g_mapTexture) { g_mapTexture->Release(); g_mapTexture = nullptr; }
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_imguiInitialized = false;
        }
        if (g_pd3dDeviceContext) {
            ID3D11RenderTargetView* nullRTV = nullptr;
            g_pd3dDeviceContext->OMSetRenderTargets(1, &nullRTV, NULL);
            g_pd3dDeviceContext->ClearState();
            g_pd3dDeviceContext->Flush();
        }
        if (g_d3d11On12Device) { g_d3d11On12Device->Release(); g_d3d11On12Device = nullptr; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
        if (g_pGameCommandQueue) { g_pGameCommandQueue->Release(); g_pGameCommandQueue = nullptr; }
    }

    inline HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
        if (g_imguiInitialized) {
            ImGui_ImplDX11_InvalidateDeviceObjects();
        }

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr) && g_imguiInitialized) {
            ImGui_ImplDX11_CreateDeviceObjects();
        }
        return hr;
    }

    inline void CloseMapUI() {
        MapRenderState::showBigMap = false;
        MapRenderState::showWaypointUI = false;
        MapRenderState::showPositionSettings = false;
    }

    inline bool IsMapWorldActive() {
        if (!g_clientInstance || !g_hasPlayer || !g_localPlayer) return false;
        __try {
            if (!g_clientInstance->isWorldActive()) return false;
            if (!g_clientInstance->getLocalPlayer()) return false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
    }

    inline bool IsNativeScreenBlockingMap(bool allowChiyanUI) {
        if (!g_clientInstance) return true;
        __try {
            if (g_clientInstance->isShowingLoadingScreen()) return true;
            if (g_clientInstance->isShowingProgressScreen()) return true;
            if (g_clientInstance->isShowingWorldProgressScreen()) return true;
            if (g_clientInstance->isShowingRealmsProgressScreen()) return true;
            if (g_clientInstance->isShowingDeathScreen()) return true;
            if (g_clientInstance->isShowingServerForm()) return true;
            if (g_clientInstance->isShowingPauseScreen()) return true;
            if (!(allowChiyanUI && MapRenderState::IsUIActive())) {
                if (g_clientInstance->isShowingMenu()) return true;
                if (!g_clientInstance->isInGameInputEnabled()) return true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }
        return false;
    }

    inline bool ShouldRenderMapOverlay() {
        if (!IsMapWorldActive()) return false;
        return !IsNativeScreenBlockingMap(true);
    }

    inline bool CanOpenMapUI() {
        if (!IsMapWorldActive()) return false;
        return !IsNativeScreenBlockingMap(false);
    }

    inline void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
        if (!g_pGameCommandQueue) {
            D3D12_COMMAND_QUEUE_DESC desc = pQueue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                g_pGameCommandQueue = pQueue;
                g_pGameCommandQueue->AddRef();
            }
        }
        oExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
    }

    inline LRESULT __stdcall WndProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (g_imguiInitialized && g_hasPlayer) {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            
            bool isTyping = false;
            if (ImGui::GetCurrentContext()) {
                isTyping = ImGui::GetIO().WantCaptureKeyboard;
            }

            if (uMsg == WM_KEYDOWN && wParam == VK_TAB) { if (!isTyping || MapRenderState::IsUIActive()) g_tabHeld = true; }
            if (uMsg == WM_KEYUP && wParam == VK_TAB) { g_tabHeld = false; }

            if (uMsg == WM_KEYDOWN && wParam == 0x4D && !isTyping) {
                if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                
                MapRenderState::showBigMap = !MapRenderState::showBigMap;

                if (MapRenderState::showBigMap) {
                    MapRenderState::bigMapOffsetX = 0.0f;
                    MapRenderState::bigMapOffsetZ = 0.0f;
                }
                return 1;
            }
            if (uMsg == WM_KEYDOWN && wParam == 0x55 && !isTyping) {
                if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                MapRenderState::showWaypointUI = !MapRenderState::showWaypointUI;
                return 1;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x49 && !isTyping) {
                if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                MapRenderState::showDeathPointUI = !MapRenderState::showDeathPointUI;
                return 1;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x4E && !isTyping) {
                if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                MapRenderState::showMiniMap = !MapRenderState::showMiniMap;
                LanguageManager::SaveConfig();
                return 1;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x59 && !isTyping) {
                if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                if (!MapRenderState::showMiniMap) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                
                // 仅在小地图显示时，才允许切换地图形状并拦截按键
                if (MapRenderState::showMiniMap) {
                    MapRenderState::isSquareMap = !MapRenderState::isSquareMap;
                    LanguageManager::SaveConfig();
                    return 1;
                }
                
                // 如果小地图隐藏，则将按键透传给游戏处理
                return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
            }

            if (MapRenderState::IsUIActive()) {
                ClipCursor(NULL);
                if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                    if (MapRenderState::showPositionSettings) {
                        MapRenderState::showPositionSettings = false;
                        MapRenderState::showBigMap = true;
                    } else {
                        MapRenderState::showBigMap = false;
                        MapRenderState::showWaypointUI = false;
                        MapRenderState::showDeathPointUI = false;
                    }
                    return 1;
                }
                if (uMsg == WM_INPUT || uMsg == WM_INPUT_DEVICE_CHANGE) return 1;
                if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return 1;
                // 当处于 UI 状态时拦截所有按键（除F11外），且不再为快捷键开特例
                // 但放行 WM_CHAR 等字符消息，以支持 IME 中文输入
                if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST && uMsg != WM_CHAR && uMsg != WM_SYSCHAR && uMsg != WM_DEADCHAR && uMsg != WM_SYSDEADCHAR && wParam != VK_F11) return 1;
                if (uMsg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
            }
        }
        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    inline LRESULT HandleWndProcFromBRD(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (!g_imguiInitialized || !g_hasPlayer) return 0;
        if (hWnd && g_hWnd != hWnd) g_hWnd = hWnd;

        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        bool isTyping = false;
        if (ImGui::GetCurrentContext()) {
            isTyping = ImGui::GetIO().WantCaptureKeyboard;
        }

        if (uMsg == WM_KEYDOWN && wParam == VK_TAB) {
            if (!isTyping || MapRenderState::IsUIActive()) g_tabHeld = true;
        }
        if (uMsg == WM_KEYUP && wParam == VK_TAB) {
            g_tabHeld = false;
        }

        if (uMsg == WM_KEYDOWN && wParam == 0x4D && !isTyping) {
            if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) return 0;
            MapRenderState::showBigMap = !MapRenderState::showBigMap;
            if (MapRenderState::showBigMap) {
                MapRenderState::bigMapOffsetX = 0.0f;
                MapRenderState::bigMapOffsetZ = 0.0f;
            }
            return 1;
        }

        if (uMsg == WM_KEYDOWN && wParam == 0x55 && !isTyping) {
            if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) return 0;
            MapRenderState::showWaypointUI = !MapRenderState::showWaypointUI;
            return 1;
        }

        if (uMsg == WM_KEYDOWN && wParam == 0x49 && !isTyping) {
            if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) return 0;
            MapRenderState::showDeathPointUI = !MapRenderState::showDeathPointUI;
            return 1;
        }

        if (uMsg == WM_KEYDOWN && wParam == 0x4E && !isTyping) {
            if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) return 0;
            MapRenderState::showMiniMap = !MapRenderState::showMiniMap;
            LanguageManager::SaveConfig();
            return 1;
        }

        if (uMsg == WM_KEYDOWN && wParam == 0x59 && !isTyping) {
            if (!MapRenderState::IsUIActive() && !CanOpenMapUI()) return 0;
            if (!MapRenderState::showMiniMap) return 0;
            MapRenderState::isSquareMap = !MapRenderState::isSquareMap;
            LanguageManager::SaveConfig();
            return 1;
        }

        if (MapRenderState::IsUIActive()) {
            ClipCursor(NULL);
            if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                if (MapRenderState::showPositionSettings) {
                    MapRenderState::tempMinimapOffsetX = MapRenderState::minimapOffsetX;
                    MapRenderState::tempMinimapOffsetY = MapRenderState::minimapOffsetY;
                    MapRenderState::showPositionSettings = false;
                    MapRenderState::showBigMap = true;
                } else {
                    MapRenderState::showBigMap = false;
                    MapRenderState::showWaypointUI = false;
                    MapRenderState::showDeathPointUI = false;
                }
                return 1;
            }
            if (uMsg == WM_INPUT || uMsg == WM_INPUT_DEVICE_CHANGE) return 1;
            if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return 1;
            if (uMsg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
            if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST && uMsg != WM_CHAR && uMsg != WM_SYSCHAR && uMsg != WM_DEADCHAR && uMsg != WM_SYSDEADCHAR && wParam != VK_F11) return 1;
        }

        return 0;
    }

    inline void InitImGuiFonts(ImGuiIO& io) {
        io.Fonts->Clear();

        ImFontConfig config;
        config.OversampleH = 1;
        config.OversampleV = 1;

        std::string baseFont = "c:\\Windows\\Fonts\\segoeui.ttf";
        if (!std::filesystem::exists(baseFont)) baseFont = "c:\\Windows\\Fonts\\arial.ttf";

        if (std::filesystem::exists(baseFont)) {
            io.Fonts->AddFontFromFileTTF(baseFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesCyrillic());
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(baseFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesVietnamese());
        } else {
            io.Fonts->AddFontDefault();
            config.MergeMode = true;
        }

        std::string cnHan = "c:\\Windows\\Fonts\\msyh.ttc";
        if (std::filesystem::exists(cnHan)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(cnHan.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
        }

        std::string jpFont = "c:\\Windows\\Fonts\\meiryo.ttc";
        if (!std::filesystem::exists(jpFont)) jpFont = "c:\\Windows\\Fonts\\msgothic.ttc";
        if (std::filesystem::exists(jpFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(jpFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesJapanese());
        }

        std::string koFont = "c:\\Windows\\Fonts\\malgun.ttf";
        if (std::filesystem::exists(koFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(koFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesKorean());
        }

        std::string thFont = "c:\\Windows\\Fonts\\leelawdb.ttf";
        if (std::filesystem::exists(thFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(thFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesThai());
        }

        std::string symbolFont = "c:\\Windows\\Fonts\\seguisym.ttf";
        if (std::filesystem::exists(symbolFont)) {
            static const ImWchar symbolRanges[] = { 0x2600, 0x26FF, 0 };
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(symbolFont.c_str(), 18.0f, &config, symbolRanges);
        }
    }

    inline void InitMapTexture() {
        if (!g_pd3dDevice) return;
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = MAP_DATA_SIZE; desc.Height = MAP_DATA_SIZE;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; desc.CPUAccessFlags = 0;

        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, NULL, &g_mapTexture))) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = desc.Format; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            if (FAILED(g_pd3dDevice->CreateShaderResourceView(g_mapTexture, &srvDesc, &g_mapTextureView))) {
                g_mapTexture->Release(); g_mapTexture = nullptr;
            }
        }
    }

    inline std::atomic<bool> g_textureBaking{false};
    inline std::atomic<bool> g_textureReadyToUpload{false};
    inline std::atomic<bool> g_partialTextureBaking{false};
    inline std::atomic<bool> g_partialTextureReadyToUpload{false};
    inline int g_partialTextureLeft = 0;
    inline int g_partialTextureTop = 0;
    inline int g_partialTextureRight = 0;
    inline int g_partialTextureBottom = 0;

    inline void UpdateMapTexture() {
        if (!g_pd3dDeviceContext || !g_mapTexture) return;

        if (g_mapDataUpdated.load() && !g_textureBaking.load() && !g_partialTextureBaking.load()) {
            g_textureBaking.store(true);
            g_mapDataUpdated.store(false);

            std::thread([]() {
                static mce::Color localColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
                static float localHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];
                static bool localWaterFlags[MAP_DATA_SIZE][MAP_DATA_SIZE];
                static float localBrightness[MAP_DATA_SIZE][MAP_DATA_SIZE];
                float centerX, centerZ;
                bool useCaveLighting;
                unsigned long long dataGeneration;
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(localColors, g_mapColors, sizeof(localColors));
                    std::memcpy(localHeights, g_mapHeights, sizeof(localHeights));
                    std::memcpy(localWaterFlags, g_mapWaterFlags, sizeof(localWaterFlags));
                    std::memcpy(localBrightness, g_mapBrightness, sizeof(localBrightness));
                    centerX = g_lastRenderX;
                    centerZ = g_lastRenderZ;
                    useCaveLighting = MapRenderState::caveMode;
                    dataGeneration = g_mapDataGeneration.load();
                }

                static uint8_t bakedData[MAP_DATA_SIZE * MAP_DATA_SIZE * 4];
                for (int x = 0; x < MAP_DATA_SIZE; x++) {
                    for (int z = 0; z < MAP_DATA_SIZE; z++) {
                        int index = (z * MAP_DATA_SIZE + x) * 4;
                        mce::Color col = localColors[x][z];

                        if (useCaveLighting && localBrightness[x][z] < 0.0f) {
                            // 墙和未加载列从扫描阶段就标为负亮度，始终保持纯黑。
                            bakedData[index] = bakedData[index + 1] = bakedData[index + 2] = 0;
                            bakedData[index + 3] = 255;
                        } else if (col.a > 0.01f) {
                            float currentY = localHeights[x][z];
                            float northY = currentY, westY = currentY;
                            bool northIsFloor = z > 0 && localColors[x][z - 1].a > 0.01f &&
                                (!useCaveLighting || localBrightness[x][z - 1] >= 0.0f);
                            bool westIsFloor = x > 0 && localColors[x - 1][z].a > 0.01f &&
                                (!useCaveLighting || localBrightness[x - 1][z] >= 0.0f);
                            if (northIsFloor && std::abs(currentY - localHeights[x][z - 1]) < 64.0f) northY = localHeights[x][z - 1];
                            if (westIsFloor && std::abs(currentY - localHeights[x - 1][z]) < 64.0f) westY = localHeights[x - 1][z];

                            float diff = (currentY - northY) * 0.15f + (currentY - westY) * 0.15f;
                            float shade = std::clamp(1.0f + diff, 0.65f, 1.25f);
                            if (useCaveLighting) {
                                float light = std::clamp(localBrightness[x][z], 0.0f, 1.0f);
                                shade *= 0.20f + 0.80f * light;
                            }
                            bakedData[index]     = (uint8_t)(std::clamp(col.r * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 1] = (uint8_t)(std::clamp(col.g * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 2] = (uint8_t)(std::clamp(col.b * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 3] = (uint8_t)(col.a * 255.0f);
                        } else {
                            bakedData[index] = bakedData[index+1] = bakedData[index+2] = bakedData[index+3] = 0;
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    if (g_mapDataGeneration.load() == dataGeneration) {
                        std::memcpy(g_textureData, bakedData, sizeof(bakedData));
                        g_textureCenterX = centerX;
                        g_textureCenterZ = centerZ;
                        g_textureReadyToUpload.store(true);
                    }
                }
                g_textureBaking.store(false);
            }).detach();
        }

        if (g_caveLightRefreshReady.load() && !MapRenderState::caveMode) {
            g_caveLightRefreshReady.store(false);
        }

        if (g_caveLightRefreshReady.load() && !g_mapDataUpdated.load() &&
            !g_textureBaking.load() && !g_partialTextureBaking.load()) {
            int left, top, right, bottom;
            int expandedLeft, expandedTop, expandedRight, expandedBottom;
            std::vector<mce::Color> localColors;
            std::vector<float> localHeights;
            std::vector<float> localBrightness;
            bool useCaveLighting = false;
            bool startPartialBake = false;
            unsigned long long dataGeneration = 0;

            {
                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                if (!g_caveLightRefreshReady.load() || !MapRenderState::caveMode) {
                    g_caveLightRefreshReady.store(false);
                } else {

                    left = std::clamp(g_caveLightRefreshLeft, 0, MAP_DATA_SIZE - 1);
                    top = std::clamp(g_caveLightRefreshTop, 0, MAP_DATA_SIZE - 1);
                    right = std::clamp(g_caveLightRefreshRight, left + 1, MAP_DATA_SIZE);
                    bottom = std::clamp(g_caveLightRefreshBottom, top + 1, MAP_DATA_SIZE);
                    expandedLeft = std::max(0, left - 1);
                    expandedTop = std::max(0, top - 1);
                    expandedRight = right;
                    expandedBottom = bottom;
                    int expandedWidth = expandedRight - expandedLeft;
                    int expandedHeight = expandedBottom - expandedTop;
                    localColors.resize(expandedWidth * expandedHeight);
                    localHeights.resize(expandedWidth * expandedHeight);
                    localBrightness.resize(expandedWidth * expandedHeight);
                    for (int z = expandedTop; z < expandedBottom; ++z) {
                        for (int x = expandedLeft; x < expandedRight; ++x) {
                            int localIndex = (z - expandedTop) * expandedWidth + x - expandedLeft;
                            localColors[localIndex] = g_mapColors[x][z];
                            localHeights[localIndex] = g_mapHeights[x][z];
                            localBrightness[localIndex] = g_mapBrightness[x][z];
                        }
                    }
                    useCaveLighting = MapRenderState::caveMode;
                    dataGeneration = g_mapDataGeneration.load();
                    g_caveLightRefreshReady.store(false);
                    g_partialTextureBaking.store(true);
                    startPartialBake = true;
                }
            }

            if (startPartialBake) std::thread([
                left,
                top,
                right,
                bottom,
                expandedLeft,
                expandedTop,
                expandedRight,
                expandedBottom,
                useCaveLighting,
                dataGeneration,
                localColors = std::move(localColors),
                localHeights = std::move(localHeights),
                localBrightness = std::move(localBrightness)
            ]() mutable {
                try {
                    int width = right - left;
                    int height = bottom - top;
                    int expandedWidth = expandedRight - expandedLeft;
                    std::vector<uint8_t> bakedData(width * height * 4);

                    for (int z = top; z < bottom; ++z) {
                        for (int x = left; x < right; ++x) {
                            int sourceIndex = (z - expandedTop) * expandedWidth + x - expandedLeft;
                            int targetIndex = ((z - top) * width + x - left) * 4;
                            mce::Color col = localColors[sourceIndex];

                            if (useCaveLighting && localBrightness[sourceIndex] < 0.0f) {
                                bakedData[targetIndex] = bakedData[targetIndex + 1] = bakedData[targetIndex + 2] = 0;
                                bakedData[targetIndex + 3] = 255;
                                continue;
                            }
                            if (col.a <= 0.01f) {
                                bakedData[targetIndex] = bakedData[targetIndex + 1] = bakedData[targetIndex + 2] = bakedData[targetIndex + 3] = 0;
                                continue;
                            }

                            float currentY = localHeights[sourceIndex];
                            float northY = currentY;
                            float westY = currentY;
                            if (z > expandedTop) {
                                int northIndex = sourceIndex - expandedWidth;
                                bool northIsFloor = localColors[northIndex].a > 0.01f &&
                                    (!useCaveLighting || localBrightness[northIndex] >= 0.0f);
                                if (northIsFloor && std::abs(currentY - localHeights[northIndex]) < 64.0f) northY = localHeights[northIndex];
                            }
                            if (x > expandedLeft) {
                                int westIndex = sourceIndex - 1;
                                bool westIsFloor = localColors[westIndex].a > 0.01f &&
                                    (!useCaveLighting || localBrightness[westIndex] >= 0.0f);
                                if (westIsFloor && std::abs(currentY - localHeights[westIndex]) < 64.0f) westY = localHeights[westIndex];
                            }

                            float diff = (currentY - northY) * 0.15f + (currentY - westY) * 0.15f;
                            float shade = std::clamp(1.0f + diff, 0.65f, 1.25f);
                            if (useCaveLighting) {
                                float light = std::clamp(localBrightness[sourceIndex], 0.0f, 1.0f);
                                shade *= 0.20f + 0.80f * light;
                            }
                            bakedData[targetIndex] = (uint8_t)(std::clamp(col.r * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[targetIndex + 1] = (uint8_t)(std::clamp(col.g * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[targetIndex + 2] = (uint8_t)(std::clamp(col.b * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[targetIndex + 3] = (uint8_t)(col.a * 255.0f);
                        }
                    }

                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    if (g_mapDataGeneration.load() == dataGeneration) {
                        for (int z = 0; z < height; ++z) {
                            uint8_t* destination = g_textureData + ((top + z) * MAP_DATA_SIZE + left) * 4;
                            std::memcpy(destination, bakedData.data() + z * width * 4, width * 4);
                        }
                        g_partialTextureLeft = left;
                        g_partialTextureTop = top;
                        g_partialTextureRight = right;
                        g_partialTextureBottom = bottom;
                        g_partialTextureReadyToUpload.store(true);
                    }
                } catch (...) {
                }
                g_partialTextureBaking.store(false);
            }).detach();
        }

        if (g_textureReadyToUpload.load()) {
            std::lock_guard<std::mutex> lock(g_mapDataMutex);
            g_pd3dDeviceContext->UpdateSubresource(g_mapTexture, 0, NULL, g_textureData, MAP_DATA_SIZE * 4, 0);
            g_textureReadyToUpload.store(false);
        }

        if (g_partialTextureReadyToUpload.load()) {
            std::lock_guard<std::mutex> lock(g_mapDataMutex);
            D3D11_BOX box = {};
            box.left = g_partialTextureLeft;
            box.top = g_partialTextureTop;
            box.front = 0;
            box.right = g_partialTextureRight;
            box.bottom = g_partialTextureBottom;
            box.back = 1;
            uint8_t* source = g_textureData + (g_partialTextureTop * MAP_DATA_SIZE + g_partialTextureLeft) * 4;
            g_pd3dDeviceContext->UpdateSubresource(g_mapTexture, 0, &box, source, MAP_DATA_SIZE * 4, 0);
            g_partialTextureReadyToUpload.store(false);
        }
    }

    inline void DrawWaypointIcon(ImDrawList* draw_list, ImVec2 center, mce::Color color, const std::string& name, bool isEdge = false) {
        float size = isEdge ? 5.5f : 8.0f; 
        ImU32 col32 = IM_COL32(color.r * 255.0f, color.g * 255.0f, color.b * 255.0f, 255);
        ImU32 outline = IM_COL32(0, 0, 0, 255); 
        
        ImVec2 pts[4] = {
            ImVec2(center.x, center.y - size),
            ImVec2(center.x + size, center.y),
            ImVec2(center.x, center.y + size),
            ImVec2(center.x - size, center.y)
        };
        
        draw_list->AddConvexPolyFilled(pts, 4, col32);
        draw_list->AddPolyline(pts, 4, outline, ImDrawFlags_Closed, 1.5f);
        
        if (!isEdge && !name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(center.x - textSize.x / 2.0f, center.y + size + 3.0f);
            draw_list->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), name.c_str()); 
            draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), name.c_str()); 
        }
    }

    inline void PointSamplerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        if (!g_pd3dDeviceContext) return;
        static ID3D11SamplerState* pPointSampler = nullptr;
        if (!pPointSampler) {
            D3D11_SAMPLER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.MipLODBias = 0.0f;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            g_pd3dDevice->CreateSamplerState(&desc, &pPointSampler);
        }
        g_pd3dDeviceContext->PSSetSamplers(0, 1, &pPointSampler);
    }

    inline void LinearSamplerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        if (!g_pd3dDeviceContext) return;
        static ID3D11SamplerState* pLinearSampler = nullptr;
        if (!pLinearSampler) {
            D3D11_SAMPLER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.MipLODBias = 0.0f;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            g_pd3dDevice->CreateSamplerState(&desc, &pLinearSampler);
        }
        g_pd3dDeviceContext->PSSetSamplers(0, 1, &pLinearSampler);
    }

    // ==========================================
    // 【极致平滑引擎】更新核心航位推测坐标
    // ==========================================
    inline void UpdateSmoothCamera() {
        static float s_lastSeenX = g_playerX;
        static float s_lastSeenZ = g_playerZ;
        static float s_velX = 0.0f;
        static float s_velZ = 0.0f;
        static auto s_lastUpdateTime = std::chrono::steady_clock::now();

        // 当底层逻辑坐标更新时，计算真实物理速度
        if (std::abs(g_playerX - s_lastSeenX) > 0.001f || std::abs(g_playerZ - s_lastSeenZ) > 0.001f) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - s_lastUpdateTime).count();
            if (dt > 0.005f && dt < 1.0f) { // 过滤异常时间跳变
                s_velX = (g_playerX - s_lastSeenX) / dt;
                s_velZ = (g_playerZ - s_lastSeenZ) / dt;
            }
            s_lastSeenX = g_playerX;
            s_lastSeenZ = g_playerZ;
            s_lastUpdateTime = now;
        }

        // 超过 150ms 没收到坐标更新，判定玩家已彻底停下，强制阻断速度消除滑步
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::duration<float>>(now - s_lastUpdateTime).count() > 0.15f) {
            s_velX = 0.0f;
            s_velZ = 0.0f;
        }

        float frameDt = ImGui::GetIO().DeltaTime;
        if (frameDt > 0.1f) frameDt = 0.1f;

        // 初始化兜底
        if (g_smoothPX == 0.0f && g_smoothPZ == 0.0f) {
            g_smoothPX = g_playerX;
            g_smoothPZ = g_playerZ;
        }

        // 1. 根据物理速度连续推测坐标 (完全脱离 20Hz 阶梯感)
        g_smoothPX += s_velX * frameDt;
        g_smoothPZ += s_velZ * frameDt;

        // 2. 软弹簧纠偏：微弱拉向真实坐标，防止漂移误差累积
        g_smoothPX += (g_playerX - g_smoothPX) * 12.0f * frameDt;
        g_smoothPZ += (g_playerZ - g_smoothPZ) * 12.0f * frameDt;

        // 3. 传送/死亡瞬间断层强行复位
        if (std::abs(g_playerX - g_smoothPX) > 10.0f || std::abs(g_playerZ - g_smoothPZ) > 10.0f) {
            g_smoothPX = g_playerX;
            g_smoothPZ = g_playerZ;
            s_velX = 0.0f;
            s_velZ = 0.0f;
        }
    }

    // ==========================================
    // [小地图 UI 渲染引擎]
    // ==========================================
    inline void RenderMiniMapBody(ImDrawList* draw_list, float cx, float cy, float IM_MAP_R) {
        float pX = g_smoothPX;
        float pZ = g_smoothPZ;

        float dx = pX - g_textureCenterX;
        float dz = pZ - g_textureCenterZ;

        float ZOOM_RADIUS = 50.0f; 
        float u = 0.5f + (dx / MAP_DATA_SIZE);
        float v = 0.5f + (dz / MAP_DATA_SIZE);
        float uvR = ZOOM_RADIUS / MAP_DATA_SIZE;

        ImVec2 uv0(u - uvR, v - uvR);
        ImVec2 uv1(u + uvR, v + uvR);
        ImVec2 mapMin(cx - IM_MAP_R, cy - IM_MAP_R);
        ImVec2 mapMax(cx + IM_MAP_R, cy + IM_MAP_R);

        float playerYaw = g_localPlayer ? g_localPlayer->getRotation().y : 0.0f;
        float yawRad = (playerYaw + 180.0f) * (3.14159265f / 180.0f);
        float cosY = std::cos(yawRad);
        float sinY = std::sin(yawRad);
        float mapSinY = -sinY;
        auto rotate = [&](float x, float y) -> ImVec2 { return ImVec2(cx + (x * cosY - y * sinY), cy + (x * sinY + y * cosY)); };
        auto rotateMap = [&](float x, float y) -> ImVec2 { return ImVec2(cx + (x * cosY - y * mapSinY), cy + (x * mapSinY + y * cosY)); };

        if (MapRenderState::isSquareMap) {
            draw_list->AddRectFilled(mapMin, mapMax, IM_COL32(20, 20, 20, 255));
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            if (MapRenderState::bigMapRotateWithPlayer) {
                float hp = IM_MAP_R * 1.414f;
                float expandedUvR = ZOOM_RADIUS * 1.414f / MAP_DATA_SIZE;
                ImVec2 eUv0(u - expandedUvR, v - expandedUvR);
                ImVec2 eUv1(u + expandedUvR, v + expandedUvR);
                ImVec2 tl = rotateMap(-hp, -hp); ImVec2 tr = rotateMap(hp, -hp);
                ImVec2 br = rotateMap(hp, hp);  ImVec2 bl = rotateMap(-hp, hp);
                draw_list->PushClipRect(mapMin, mapMax, true);
                draw_list->AddImageQuad((void*)g_mapTextureView, tl, tr, br, bl, eUv0, ImVec2(eUv1.x, eUv0.y), eUv1, ImVec2(eUv0.x, eUv1.y));
                draw_list->PopClipRect();
            } else {
                draw_list->AddImage((void*)g_mapTextureView, mapMin, mapMax, uv0, uv1, IM_COL32_WHITE);
            }
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
            draw_list->AddRect(mapMin, mapMax, IM_COL32(30, 30, 30, 255), 0.0f, 0, 2.0f);
        } else {
            draw_list->AddCircleFilled(ImVec2(cx, cy), IM_MAP_R, IM_COL32(20, 20, 20, 255), 64);
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            if (MapRenderState::bigMapRotateWithPlayer) {
                draw_list->PushTexture((void*)g_mapTextureView);
                const int segs = 48;
                unsigned int baseIdx = draw_list->_VtxCurrentIdx;
                draw_list->PrimReserve(segs * 3, segs + 2);
                ImVec2 ctr_uv((uv0.x + uv1.x) * 0.5f, (uv0.y + uv1.y) * 0.5f);
                draw_list->PrimWriteVtx(ImVec2(cx, cy), ctr_uv, IM_COL32_WHITE);
                unsigned int prevIdx = baseIdx + 1;
                for (int i = 0; i <= segs; i++) {
                    float a = (float)i / (float)segs * 2.0f * 3.14159265f;
                    float u_rel = 0.5f + 0.5f * cos(a + yawRad);
                    float v_rel = 0.5f + 0.5f * sin(a + yawRad);
                    draw_list->PrimWriteVtx(
                        ImVec2(cx + IM_MAP_R * cos(a), cy + IM_MAP_R * sin(a)),
                        ImVec2(uv0.x + (uv1.x - uv0.x) * u_rel, uv0.y + (uv1.y - uv0.y) * v_rel),
                        IM_COL32_WHITE
                    );
                    unsigned int curIdx = baseIdx + 1 + i;
                    if (i > 0) {
                        draw_list->PrimWriteIdx((ImDrawIdx)(int)(baseIdx + 0));
                        draw_list->PrimWriteIdx((ImDrawIdx)(int)prevIdx);
                        draw_list->PrimWriteIdx((ImDrawIdx)(int)curIdx);
                    }
                    prevIdx = curIdx;
                }
                draw_list->PopTexture();
            } else {
                draw_list->AddImageRounded((void*)g_mapTextureView, mapMin, mapMax, uv0, uv1, IM_COL32_WHITE, IM_MAP_R);
            }
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
            draw_list->AddCircle(ImVec2(cx, cy), IM_MAP_R, IM_COL32(30, 30, 30, 255), 64, 2.0f);
        }

        char coordBuf[64];
        snprintf(coordBuf, sizeof(coordBuf), "%d, %d, %d", g_playerBlockX, (int)g_playerY, g_playerBlockZ);
        ImFont* infoFont = ImGui::GetFont();
        float infoFontSize = std::clamp(IM_MAP_R * 0.15f, 16.0f, 24.0f);
        float infoLineGap = infoFontSize * 1.32f;
        auto calcInfoTextSize = [&](const char* text) {
            return infoFont->CalcTextSizeA(infoFontSize, 1.0e9f, 0.0f, text);
        };
        auto addInfoText = [&](ImVec2 pos, ImU32 color, const char* text) {
            draw_list->AddText(infoFont, infoFontSize, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 210), text);
            draw_list->AddText(infoFont, infoFontSize, pos, color, text);
        };
        ImVec2 coordSize = calcInfoTextSize(coordBuf);
        ImVec2 coordPos(cx - coordSize.x / 2, cy + IM_MAP_R + infoFontSize * 1.15f);
        addInfoText(coordPos, IM_COL32(255, 255, 255, 255), coordBuf);

        std::string biomeStr = MapRenderState::currentBiomeName;
        size_t startPos = biomeStr.find("(");
        size_t endPos = biomeStr.find(")");
        if (startPos != std::string::npos && endPos != std::string::npos) {
            biomeStr = biomeStr.substr(startPos + 1, endPos - startPos - 1);
        }
        ImVec2 biomeSize = calcInfoTextSize(biomeStr.c_str());
        ImVec2 biomePos(cx - biomeSize.x / 2, coordPos.y + infoLineGap);
        addInfoText(biomePos, IM_COL32(220, 220, 220, 255), biomeStr.c_str());

        if (MapRenderState::bigMapRotateWithPlayer) {
            float compassR = IM_MAP_R + 16.0f;
            auto compassPos = [&](float nx, float nz) -> ImVec2 {
                return ImVec2(cx + (nx * cosY - nz * mapSinY) * compassR, cy + (nx * mapSinY + nz * cosY) * compassR);
            };
            draw_list->AddText(ImVec2(compassPos(0, -1).x - 8, compassPos(0, -1).y - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_N"));
            draw_list->AddText(ImVec2(compassPos(0, 1).x - 8, compassPos(0, 1).y - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_S"));
            draw_list->AddText(ImVec2(compassPos(1, 0).x - 8, compassPos(1, 0).y - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_E"));
            draw_list->AddText(ImVec2(compassPos(-1, 0).x - 8, compassPos(-1, 0).y - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_W"));
        } else {
            draw_list->AddText(ImVec2(cx - 8, cy - IM_MAP_R - 20), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_N"));
            draw_list->AddText(ImVec2(cx - 8, cy + IM_MAP_R - 2), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_S"));
            draw_list->AddText(ImVec2(cx + IM_MAP_R + 4, cy - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_E"));
            draw_list->AddText(ImVec2(cx - IM_MAP_R - 20, cy - 9), IM_COL32(220, 220, 255, 255), LanguageManager::GetText("COMPASS_W"));
        }

        {
            char modeBuf[64] = {};
            if (MapRenderState::caveMode) {
                snprintf(modeBuf, sizeof(modeBuf), LanguageManager::GetText("CAVE_MODE"), MapRenderState::caveScanY);
                ImVec2 modeSize = calcInfoTextSize(modeBuf);
                ImVec2 modePos(cx - modeSize.x / 2, biomePos.y + infoLineGap);
                addInfoText(modePos, IM_COL32(255, 200, 50, 255), modeBuf);
            }
        }

        static std::vector<RadarEntity> s_cachedEntities;
        if (g_radarUpdated.load()) {
            s_cachedEntities = g_radarEntities;
            g_radarUpdated.store(false);
        }

        float scale = IM_MAP_R / ZOOM_RADIUS; 
        for (const auto& ent : s_cachedEntities) {
            float edx = ent.x - g_playerX;
            float edz = ent.z - g_playerZ;
            if (edx * edx + edz * edz < 4.0f) continue;
            edx = ent.x - pX;
            edz = ent.z - pZ;
            float ex, ez;
            if (MapRenderState::bigMapRotateWithPlayer) {
                float rx = edx * cosY - edz * mapSinY;
                float rz = edx * mapSinY + edz * cosY;
                ex = cx + rx * scale;
                ez = cy + rz * scale;
            } else {
                ex = cx + edx * scale;
                ez = cy + edz * scale;
            }
            
            bool inBounds = false;
            if (MapRenderState::isSquareMap) {
                inBounds = (std::abs(ex - cx) <= IM_MAP_R && std::abs(ez - cy) <= IM_MAP_R);
            } else {
                float distSq = (ex - cx) * (ex - cx) + (ez - cy) * (ez - cy);
                inBounds = (distSq <= IM_MAP_R * IM_MAP_R);
            }

            if (inBounds) {
                ID3D11ShaderResourceView* faceSrv = nullptr;
                if (ent.type == 0) {
                    faceSrv = GetOrCreatePlayerHeadTexture(ent.uuid);
                    if (!faceSrv) faceSrv = GetOrCreateFaceTexture("player");
                } else if (ent.type == 3) {
                } else {
                    faceSrv = GetOrCreateFaceTexture(ent.entityType);
                }

                if (faceSrv) {
                    float hs = 6.0f * (IM_MAP_R / 135.0f);
                    if (ent.type == 0) hs *= 1.3f;
                    if (g_tabHeld) hs *= 2.0f;
                    draw_list->AddImage((void*)faceSrv, ImVec2(ex - hs, ez - hs), ImVec2(ex + hs, ez + hs));
                } else {
                    ImU32 col;
                    if (ent.type == 0) col = IM_COL32(255, 255, 255, 255);
                    else if (ent.type == 1) col = IM_COL32(255, 50, 50, 255);
                    else if (ent.type == 2) col = IM_COL32(50, 255, 50, 255);
                    else col = IM_COL32(255, 255, 50, 255);
                    draw_list->AddRectFilled(ImVec2(ex - 2, ez - 2), ImVec2(ex + 2, ez + 2), col);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
            for (const auto& wp : WaypointManager::g_waypoints) {
                if (!wp.enabled) continue;
                
                float wDx = wp.x - pX;
                float wDz = wp.z - pZ;
                float physicalDist = std::sqrt(wDx * wDx + wDz * wDz);
                if (physicalDist < 0.001f) physicalDist = 0.001f;

                float ex, ez, rwDx, rwDz;
                if (MapRenderState::bigMapRotateWithPlayer) {
                    rwDx = wDx * cosY - wDz * mapSinY;
                    rwDz = wDx * mapSinY + wDz * cosY;
                    ex = cx + rwDx * scale;
                    ez = cy + rwDz * scale;
                } else {
                    rwDx = wDx; rwDz = wDz;
                    ex = cx + wDx * scale;
                    ez = cy + wDz * scale;
                }
                
                bool inMap = false;
                float edgeX = cx, edgeZ = cy;

                if (MapRenderState::isSquareMap) {
                    if (std::abs(ex - cx) <= IM_MAP_R && std::abs(ez - cy) <= IM_MAP_R) {
                        inMap = true;
                    } else {
                        float maxDist = std::max(std::abs(rwDx), std::abs(rwDz));
                        edgeX = cx + (rwDx / maxDist) * IM_MAP_R;
                        edgeZ = cy + (rwDz / maxDist) * IM_MAP_R;
                    }
                } else {
                    float rDist = std::sqrt(rwDx * rwDx + rwDz * rwDz);
                    if (rDist <= ZOOM_RADIUS) {
                        inMap = true;
                    } else {
                        edgeX = cx + (rwDx / rDist) * IM_MAP_R;
                        edgeZ = cy + (rwDz / rDist) * IM_MAP_R;
                    }
                }

                if (inMap) {
                    DrawWaypointIcon(draw_list, ImVec2(ex, ez), mce::Color(wp.r, wp.g, wp.b, 1.0f), wp.name, false);
                } else {
                    DrawWaypointIcon(draw_list, ImVec2(edgeX, edgeZ), mce::Color(wp.r, wp.g, wp.b, 1.0f), "", true);
                }
            }
        }

        if (MapRenderState::bigMapRotateWithPlayer) {
            draw_list->AddTriangleFilled(ImVec2(cx, cy - 10.0f), ImVec2(cx - 7.0f, cy + 10.0f), ImVec2(cx + 7.0f, cy + 10.0f), IM_COL32(0, 0, 0, 255));
            draw_list->AddTriangleFilled(ImVec2(cx, cy - 8.0f), ImVec2(cx - 5.0f, cy + 8.0f), ImVec2(cx + 5.0f, cy + 8.0f), IM_COL32(220, 20, 20, 255));
        } else {
            draw_list->AddTriangleFilled(rotate(0, -10.0f), rotate(-7.0f, 10.0f), rotate(7.0f, 10.0f), IM_COL32(0, 0, 0, 255));
            draw_list->AddTriangleFilled(rotate(0, -8.0f), rotate(-5.0f, 8.0f), rotate(5.0f, 8.0f), IM_COL32(220, 20, 20, 255));
        }
    }

    inline void RenderImGuiXaeroMap() {
        if (!MapRenderState::showMiniMap) return; 
        if (!g_mapTextureView) return;
        UpdateMapTexture();

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        float IM_MAP_R = MapRenderState::minimapSize; 
        float IM_MAP_MARGIN = 20.0f;
        float ox = MapRenderState::showPositionSettings ? MapRenderState::tempMinimapOffsetX : MapRenderState::minimapOffsetX;
        float oy = MapRenderState::showPositionSettings ? MapRenderState::tempMinimapOffsetY : MapRenderState::minimapOffsetY;
        float dsx = ImGui::GetIO().DisplaySize.x;
        float dsy = ImGui::GetIO().DisplaySize.y;
        ox = std::clamp(ox, -(dsx / 2 - IM_MAP_R), dsx / 2 - IM_MAP_R);
        oy = std::clamp(oy, -(dsy / 2 - IM_MAP_R), dsy / 2 - IM_MAP_R);
        float cx = std::floor(dsx / 2 + ox);
        float cy = std::floor(dsy / 2 + oy);

        RenderMiniMapBody(draw_list, cx, cy, IM_MAP_R);
    }

    inline void UpdateRegionTexture(uint64_t hash, int& texCount) {
        if (!g_pd3dDevice || !g_pd3dDeviceContext) return;
        
        bool isKnown = (g_regionTextures.find(hash) != g_regionTextures.end());
        
        // 1. 【极速状态探测】直接窥探底层 IO 状态，如果缺失直接排队，绝不阻塞主线程，完美修复加载断层
        bool isLoadedAndDirty = false;
        {
            std::lock_guard<std::mutex> lock(MapCacheManager::g_cacheMutex);
            auto it = MapCacheManager::g_loadedRegions.find(hash);
            if (it == MapCacheManager::g_loadedRegions.end()) {
                MapCacheManager::g_loadedRegions[hash] = nullptr;
                MapCacheManager::g_loadQueue.push_back(hash);
                return; // 刚排队，直接闪退
            } else if (it->second != nullptr) {
                isLoadedAndDirty = it->second->textureDirty;
            }
        }

        // 数据没脏就不需要更新
        if (!isLoadedAndDirty) return;

        // 需要建图但本帧建图配额（提升至4以加快大后期渲染）已满，直接返回（保留 Dirty 给下帧）
        if (!isKnown && texCount >= 4) return;

        // 内存对齐以支持极速 64 位空域探测
        alignas(8) static uint8_t tempBuffer[256 * 256 * 4];
        if (MapCacheManager::FetchRegionTextureData(hash, tempBuffer)) {
            
            // 【DrawCall级优化核心】透明区块免疫技术：如果这片区域完全没探索过（纯透明），彻底免渲染！
            bool isEmpty = true;
            uint64_t* ptr64 = (uint64_t*)tempBuffer;
            for (int i = 0; i < (256 * 256 * 4) / 8; ++i) {
                if (ptr64[i] != 0) { isEmpty = false; break; }
            }

            // 若原本未知，或原本是空贴图但现在有了数据
            if (!isKnown || (isKnown && g_regionTextures[hash] == nullptr && !isEmpty)) {
                if (isEmpty) {
                    // 标记为空贴图，不占用显存，极大幅度缩减 ImGui DrawCall 数量
                    g_regionTextures[hash] = nullptr;
                    g_regionSRVs[hash] = nullptr;
                } else {
                    texCount++;
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = 256; desc.Height = 256;
                    desc.MipLevels = 1; desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    
                    D3D11_SUBRESOURCE_DATA initData = {};
                    initData.pSysMem = tempBuffer;
                    initData.SysMemPitch = 256 * 4;
                    
                    ID3D11Texture2D* tex = nullptr;
                    ID3D11ShaderResourceView* srv = nullptr;
                    
                    if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex))) {
                        g_pd3dDevice->CreateShaderResourceView(tex, NULL, &srv);
                        g_regionTextures[hash] = tex;
                        g_regionSRVs[hash] = srv;
                    }
                }
            } else if (g_regionTextures[hash] != nullptr) {
                g_pd3dDeviceContext->UpdateSubresource(g_regionTextures[hash], 0, NULL, tempBuffer, 256 * 4, 0);
            }
        }
    }

    // ==========================================
    // 提取公共重命名模态弹窗组件
    // ==========================================
    inline void RenderRenameModal(const char* modalId, std::string& wpId, bool& trigger) {
        if (trigger) {
            ImGui::OpenPopup(modalId);
            trigger = false;
        }
        if (ImGui::BeginPopupModal(modalId, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char renameBuf[256] = "";
            static bool initialized = false;
            
            Waypoint targetWp;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for(auto& w : WaypointManager::g_waypoints) {
                    if(w.id == wpId) { targetWp = w; found = true; break; }
                }
            }
            
            if (!found) {
                ImGui::CloseCurrentPopup();
            } else {
                if (!initialized) {
                    snprintf(renameBuf, sizeof(renameBuf), "%s", targetWp.name.c_str());
                    initialized = true;
                }
                
                ImGui::InputText(LanguageManager::GetText("WP_NAME"), renameBuf, sizeof(renameBuf));
                ImGui::Spacing();
                
                PushOreButtonStyle(OreButtonKind::Success);
                if (ImGui::Button(LanguageManager::GetText("WP_SAVE"), ImVec2(120, 0))) {
                    {
                        std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                        for(auto& w : WaypointManager::g_waypoints) {
                            if(w.id == wpId) { 
                                w.name = renameBuf; 
                                break; 
                            }
                        }
                    }
                    WaypointManager::SaveWaypoints();
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }
                PopOreButtonStyle();
                ImGui::SameLine();
                PushOreButtonStyle(OreButtonKind::Default);
                if (ImGui::Button(LanguageManager::GetText("WP_CANCEL"), ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }
                PopOreButtonStyle();
            }
            ImGui::EndPopup();
        }
    }

    inline void RenderImGuiBigMap() {
        if (MapRenderState::clearGPUCache.load()) {
            for(auto& p : g_regionSRVs) if(p.second) p.second->Release();
            g_regionSRVs.clear();
            for(auto& p : g_regionTextures) if(p.second) p.second->Release();
            g_regionTextures.clear();
            MapRenderState::clearGPUCache.store(false);
        }

        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | 
                                        ImGuiWindowFlags_NoBackground;
        
        ImGui::Begin("BigMapCanvas", nullptr, window_flags);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        bool isHoveringCanvas = ImGui::IsWindowHovered();

        static bool s_isDraggingMap = false;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isHoveringCanvas) {
            s_isDraggingMap = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            s_isDraggingMap = false;
        }

        if (s_isDraggingMap) {
            MapRenderState::bigMapOffsetX += io.MouseDelta.x;
            MapRenderState::bigMapOffsetZ += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f && isHoveringCanvas) {
            float oldZoom = MapRenderState::bigMapZoom;
            float zoomSpeed = 0.15f * oldZoom;
            MapRenderState::bigMapZoom += io.MouseWheel * zoomSpeed;
            if (MapRenderState::bigMapZoom < 0.2f) MapRenderState::bigMapZoom = 0.2f;
            if (MapRenderState::bigMapZoom > 40.0f) MapRenderState::bigMapZoom = 40.0f;
            
            float k = MapRenderState::bigMapZoom / oldZoom;
            float cx = io.DisplaySize.x * 0.5f + MapRenderState::bigMapOffsetX;
            float cy = io.DisplaySize.y * 0.5f + MapRenderState::bigMapOffsetZ;
            float dx = io.MousePos.x - cx;
            float dy = io.MousePos.y - cy;
            
            MapRenderState::bigMapOffsetX -= dx * (k - 1.0f);
            MapRenderState::bigMapOffsetZ -= dy * (k - 1.0f);
        }

        draw_list->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(20, 20, 20, 255));

        float cx = io.DisplaySize.x * 0.5f;
        float cy = io.DisplaySize.y * 0.5f;
        
        float minWx = g_smoothPX - (cx + MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float maxWx = g_smoothPX + (cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float minWz = g_smoothPZ - (cy + MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;
        float maxWz = g_smoothPZ + (cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        int startRx = (int)std::floor(minWx / 256.0f);
        int endRx   = (int)std::floor(maxWx / 256.0f);
        int startRz = (int)std::floor(minWz / 256.0f);
        int endRz   = (int)std::floor(maxWz / 256.0f);

        int texturesCreatedThisFrame = 0;
        {
            static int s_vramGcTimer = 0;
            if (++s_vramGcTimer > 300) {
                s_vramGcTimer = 0;
                std::vector<uint64_t> keysToErase;
                for (auto& p : g_regionSRVs) {
                    int rx = (int)(p.first >> 32);
                    int rz = (int)(p.first & 0xFFFFFFFF);
                    if (rx < startRx - 5 || rx > endRx + 5 || rz < startRz - 5 || rz > endRz + 5) {
                        keysToErase.push_back(p.first);
                    }
                }
                for (uint64_t k : keysToErase) {
                    if (g_regionSRVs[k]) g_regionSRVs[k]->Release();
                    g_regionSRVs.erase(k);
                    if (g_regionTextures[k]) g_regionTextures[k]->Release();
                    g_regionTextures.erase(k);
                    MapCacheManager::MarkTextureDirty(k);
                }
            }

            draw_list->AddCallback(PointSamplerCallback, nullptr);
            
            if (MapRenderState::caveMode) {
                float mmCx = cx + MapRenderState::bigMapOffsetX;
                float mmCy = cy + MapRenderState::bigMapOffsetZ;
                float mmSz = 513.0f * MapRenderState::bigMapZoom;
                draw_list->AddImage((void*)g_mapTextureView,
                    ImVec2(mmCx - mmSz * 0.5f, mmCy - mmSz * 0.5f),
                    ImVec2(mmCx + mmSz * 0.5f, mmCy + mmSz * 0.5f));
            } else {
                for (int rx = startRx; rx <= endRx; rx++) {
                    for (int rz = startRz; rz <= endRz; rz++) {
                        uint64_t hash = MapCacheManager::GetRegionHash(rx, rz);
                        UpdateRegionTexture(hash, texturesCreatedThisFrame);

                        if (g_regionSRVs.find(hash) != g_regionSRVs.end()) {
                            // 【渲染管线减负】只有包含实际像素的非空贴图才会被加入 ImGui 的 DrawCall 绘制队列！
                            // 极大减负显卡在微缩大地图时的渲染压力，实现绝对满帧体验。
                            if (g_regionSRVs[hash] != nullptr) {
                                float sx_min = std::floor(cx + (rx * 256.0f - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                                float sy_min = std::floor(cy + (rz * 256.0f - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                                float sx_max = std::floor(cx + ((rx + 1) * 256.0f - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                                float sy_max = std::floor(cy + ((rz + 1) * 256.0f - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                                
                                draw_list->AddImage((void*)g_regionSRVs[hash], ImVec2(sx_min, sy_min), ImVec2(sx_max, sy_max));
                            }
                        }
                    }
                }
            }
            
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
        }
        
        float px = cx + MapRenderState::bigMapOffsetX;
        float py = cy + MapRenderState::bigMapOffsetZ;
        
        float yawRad = (g_playerYaw + 180.0f) * (3.14159265f / 180.0f); 
        float cosY = std::cos(yawRad);
        float sinY = std::sin(yawRad);
        auto rotate = [&](float x, float y) -> ImVec2 { 
            return ImVec2(px + (x * cosY - y * sinY), py + (x * sinY + y * cosY)); 
        };

        draw_list->AddTriangleFilled(rotate(0, -10.0f), rotate(-7.0f, 10.0f), rotate(7.0f, 10.0f), IM_COL32(0, 0, 0, 255));
        draw_list->AddTriangleFilled(rotate(0, -8.0f), rotate(-5.0f, 8.0f), rotate(5.0f, 8.0f), IM_COL32(220, 20, 20, 255));

        if (g_tabHeld) {
            static std::vector<RadarEntity> s_cachedEntities;
            if (g_radarUpdated.load()) {
                s_cachedEntities = g_radarEntities;
                g_radarUpdated.store(false);
            }
            for (const auto& ent : s_cachedEntities) {
                float dxSelf = ent.x - g_playerX;
                float dzSelf = ent.z - g_playerZ;
                if (dxSelf * dxSelf + dzSelf * dzSelf < 4.0f) continue;
                float wx = cx + (ent.x - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX;
                float wz = cy + (ent.z - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ;
                if (wx < -50.0f || wx > io.DisplaySize.x + 50.0f || wz < -50.0f || wz > io.DisplaySize.y + 50.0f) continue;

                ID3D11ShaderResourceView* faceSrv = nullptr;
                if (ent.type == 0) {
                    faceSrv = GetOrCreatePlayerHeadTexture(ent.uuid);
                    if (!faceSrv) faceSrv = GetOrCreateFaceTexture("player");
                } else if (ent.type != 3) {
                    faceSrv = GetOrCreateFaceTexture(ent.entityType);
                }

                float is = 8.0f;
                if (ent.type == 0) is *= 1.3f;

                if (faceSrv) {
                    draw_list->AddImage((void*)faceSrv, ImVec2(wx - is, wz - is), ImVec2(wx + is, wz + is));
                } else {
                    ImU32 col;
                    if (ent.type == 0) col = IM_COL32(255, 255, 255, 255);
                    else if (ent.type == 1) col = IM_COL32(255, 50, 50, 255);
                    else if (ent.type == 2) col = IM_COL32(50, 255, 50, 255);
                    else col = IM_COL32(255, 255, 50, 255);
                    draw_list->AddRectFilled(ImVec2(wx - 2, wz - 2), ImVec2(wx + 2, wz + 2), col);
                }
            }
        }

        static std::string selectedWpId = "";
        static bool triggerWpMenu = false;
        {
            std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
            for (const auto& wp : WaypointManager::g_waypoints) {
                if (!wp.enabled) continue;
                
                float wx = cx + (wp.x - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX;
                float wz = cy + (wp.z - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ;
                
                if (wx > -50.0f && wx < io.DisplaySize.x + 50.0f && wz > -50.0f && wz < io.DisplaySize.y + 50.0f) {
                    DrawWaypointIcon(draw_list, ImVec2(wx, wz), mce::Color(wp.r, wp.g, wp.b, 1.0f), wp.name, false);
                    
                    float distSq = (io.MousePos.x - wx) * (io.MousePos.x - wx) + (io.MousePos.y - wz) * (io.MousePos.y - wz);
                    if (distSq <= 144.0f && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        selectedWpId = wp.id;
                        triggerWpMenu = true;
                    }
                }
            }
        }

        float hoverWx = g_smoothPX + (io.MousePos.x - cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float hoverWz = g_smoothPZ + (io.MousePos.y - cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        char infoBuf[256];
        snprintf(infoBuf, sizeof(infoBuf), LanguageManager::GetText("BIGMAP_TITLE"), MapRenderState::bigMapZoom);
        draw_list->AddText(ImVec2(20, 20), IM_COL32(255, 200, 50, 255), infoBuf);
        draw_list->AddText(ImVec2(20, 45), IM_COL32(200, 200, 200, 255), LanguageManager::GetText("BIGMAP_HELP"));
        
        snprintf(infoBuf, sizeof(infoBuf), LanguageManager::GetText("CURSOR_POS"), (int)std::floor(hoverWx), (int)std::floor(hoverWz));
        ImVec2 textSize = ImGui::CalcTextSize(infoBuf);
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - textSize.x / 2 - 15, io.DisplaySize.y - 45), 
                                 ImVec2(io.DisplaySize.x / 2 + textSize.x / 2 + 15, io.DisplaySize.y - 10), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(ImVec2(io.DisplaySize.x / 2 - textSize.x / 2, io.DisplaySize.y - 35), IM_COL32(255, 255, 255, 255), infoBuf);

        char biomeBuf[512];
        snprintf(biomeBuf, sizeof(biomeBuf), LanguageManager::GetText("BIOME_LABEL"), MapRenderState::currentBiomeName.c_str());
        ImVec2 biomeTextSize = ImGui::CalcTextSize(biomeBuf);
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2 - 20, 15), 
                                 ImVec2(io.DisplaySize.x / 2 + biomeTextSize.x / 2 + 20, 50), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2, 25), IM_COL32(180, 255, 180, 255), biomeBuf);

        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 290, 20));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
        ImGui::BeginChild("MapSidebar", ImVec2(270, 190), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), LanguageManager::GetText("SIDEBAR_PLAYER_STATUS"));
        ImGui::Separator();
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_X"), g_playerBlockX);
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_Y"), (int)g_playerY);
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_Z"), g_playerBlockZ);
        
        ImGui::Spacing(); ImGui::Spacing();
        
        // 并排摆放 [视角回中] 主按钮与 [⚙] 齿轮设置按钮
        float availWidth = ImGui::GetContentRegionAvail().x;
        PushOreButtonStyle(OreButtonKind::Primary);
        if (ImGui::Button(LanguageManager::GetText("CENTER_CAMERA"), ImVec2(availWidth - 42.0f, 35.0f))) {
            MapRenderState::bigMapOffsetX = 0.0f;
            MapRenderState::bigMapOffsetZ = 0.0f;
        }
        PopOreButtonStyle();
        ImGui::SameLine();
        
        PushOreButtonStyle(OreButtonKind::Warning);
        if (ImGui::Button("\xe2\x9a\x99", ImVec2(35.0f, 35.0f))) {
            ImGui::OpenPopup("SettingsPopup");
        }
        PopOreButtonStyle();
        
        ImGui::SetNextWindowSize(ImVec2(380, 500));
        if (ImGui::BeginPopup("SettingsPopup")) {
            OreSectionHeader(LanguageManager::GetText("SIDEBAR_OPS"));
            
            if (ImGui::Checkbox(LanguageManager::GetText("SHOW_MINIMAP"), &MapRenderState::showMiniMap)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::Checkbox(LanguageManager::GetText("SQUARE_MINIMAP"), &MapRenderState::isSquareMap)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::Checkbox(LanguageManager::GetText("ROTATE_MAP"), &MapRenderState::bigMapRotateWithPlayer)) {
                LanguageManager::SaveConfig();
            }
            ImGui::Spacing();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::SliderFloat(LanguageManager::GetText("MINIMAP_SIZE"), &MapRenderState::minimapSize, 60.0f, 300.0f, "%.0f")) {
                LanguageManager::SaveConfig();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();

            OreSectionHeader(LanguageManager::GetText("MCOMPASS_TITLE"));
            ImGui::TextColored(ExternalCompassStatusColor(), LanguageManager::GetText("MCOMPASS_STATUS"), ExternalCompassStatusText());
            if (ImGui::Checkbox(LanguageManager::GetText("MCOMPASS_ENABLE"), &MapRenderState::externalCompassEnabled)) {
                LanguageManager::SaveConfig();
                ExternalCompassSync::NotifyConfigChanged();
            }

            if (MapRenderState::externalCompassEnabled) {
                static bool externalCompassNameLoaded = false;
                static char externalCompassNameBuf[64] = "MCOMPASS";
                if (!externalCompassNameLoaded) {
                    strncpy_s(externalCompassNameBuf, MapRenderState::externalCompassDeviceName.c_str(), _TRUNCATE);
                    externalCompassNameLoaded = true;
                }
                ImGui::PushItemWidth(-1);
                ImGui::InputText(LanguageManager::GetText("MCOMPASS_DEVICE_NAME"), externalCompassNameBuf, sizeof(externalCompassNameBuf));
                if (ImGui::SliderInt(LanguageManager::GetText("MCOMPASS_INTERVAL"), &MapRenderState::externalCompassIntervalMs, 20, 1000)) {
                    LanguageManager::SaveConfig();
                }
                if (ImGui::SliderFloat(LanguageManager::GetText("MCOMPASS_MIN_DELTA"), &MapRenderState::externalCompassMinDelta, 0.1f, 10.0f, "%.1f deg")) {
                    LanguageManager::SaveConfig();
                }
                PushOreButtonStyle(OreButtonKind::Primary);
                if (ImGui::Button(LanguageManager::GetText("MCOMPASS_APPLY"), ImVec2(ImGui::GetContentRegionAvail().x, 28.0f))) {
                    MapRenderState::externalCompassDeviceName = externalCompassNameBuf;
                    LanguageManager::SaveConfig();
                    ExternalCompassSync::NotifyConfigChanged();
                }
                PopOreButtonStyle();
                ImGui::PopItemWidth();
            }
            ImGui::Spacing();

            OreSectionHeader(LanguageManager::GetText("LANG_SELECT"));
            std::string previewName = LanguageManager::g_currentLanguage;
            for (const auto& p : LanguageManager::g_availableLanguages) {
                if (p.first == LanguageManager::g_currentLanguage) {
                    previewName = p.second;
                    break;
                }
            }
            
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##LangSelectCombo", previewName.c_str())) {
                for (const auto& p : LanguageManager::g_availableLanguages) {
                    bool isSelected = (LanguageManager::g_currentLanguage == p.first);
                    if (ImGui::Selectable(p.second.c_str(), isSelected)) {
                        LanguageManager::g_currentLanguage = p.first;
                        LanguageManager::LoadLanguage(p.first);
                        LanguageManager::SaveConfig();
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button(LanguageManager::GetText("ADJUST_POSITION"), ImVec2(ImGui::GetContentRegionAvail().x, 35.0f))) {
                MapRenderState::tempMinimapOffsetX = MapRenderState::minimapOffsetX;
                MapRenderState::tempMinimapOffsetY = MapRenderState::minimapOffsetY;
                MapRenderState::showPositionSettings = true;
                MapRenderState::showBigMap = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        static float rcWorldX = 0.0f;
        static float rcWorldZ = 0.0f;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            rcWorldX = hoverWx;
            rcWorldZ = hoverWz;
            ImGui::OpenPopup("BigMapContextMenu");
        }

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        if (ImGui::BeginPopup("BigMapContextMenu")) {
            int bx = (int)std::floor(rcWorldX);
            int bz = (int)std::floor(rcWorldZ);
            
            // 【完成需求2】全屏大地图添加地标时，瞬间获取真正的底层地形表面高度
            int by = 320; 
            if (g_clientInstance) {
                BlockSource* region = g_clientInstance->getRegion();
                if (region) {
                    short topY = region->getAboveTopSolidBlock(bx, bz, true, true);
                    if (topY > -60 && topY < 319) {
                        by = (int)topY + 1; // 地形如果已经加载过，立刻抓取地表最高点！
                    }
                }
            }

            ImVec2 titleSize = ImGui::CalcTextSize(LanguageManager::GetText("CONTEXT_TITLE"));
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
            ImGui::Text("%s", LanguageManager::GetText("CONTEXT_TITLE"));
            ImGui::Separator();

            char chunkBuf[64]; snprintf(chunkBuf, sizeof(chunkBuf), LanguageManager::GetText("CHUNK_POS"), bx >> 4, bz >> 4);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(chunkBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", chunkBuf);

            char blockBuf[64]; snprintf(blockBuf, sizeof(blockBuf), LanguageManager::GetText("BLOCK_POS"), bx, by, bz);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(blockBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", blockBuf);
            ImGui::Separator();

            if (ImGui::Selectable(LanguageManager::GetText("COPY_COORDS"))) {
                char buf[128]; snprintf(buf, sizeof(buf), "%d %d %d", bx, by, bz);
                ImGui::SetClipboardText(buf);
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable(LanguageManager::GetText("CREATE_WAYPOINT"))) {
                MapRenderState::addWaypointX = bx;
                MapRenderState::addWaypointY = by;
                MapRenderState::addWaypointZ = bz;
                MapRenderState::triggerAddWaypoint = true;
                MapRenderState::showWaypointUI = true;
            }
            
            if (ImGui::Selectable(LanguageManager::GetText("TELEPORT_HERE"))) {
                MapRenderState::tpTargetX = (float)bx + 0.5f;
                MapRenderState::tpTargetY = (float)by; 
                MapRenderState::tpTargetZ = (float)bz + 0.5f;
                MapRenderState::triggerTeleport.store(true);
                MapRenderState::showBigMap = false; 
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable(LanguageManager::GetText("OPEN_WP_MENU"))) {
                MapRenderState::showWaypointUI = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);

        if (triggerWpMenu) {
            ImGui::OpenPopup("WaypointContextMenu");
            triggerWpMenu = false;
        }

        static std::string bigMapRenameId = "";
        static bool bigMapTriggerRename = false;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        if (ImGui::BeginPopup("WaypointContextMenu")) {
            Waypoint targetWp;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for(auto& w : WaypointManager::g_waypoints) {
                    if(w.id == selectedWpId) {
                        targetWp = w;
                        found = true; 
                        break;
                    }
                }
            }

            if (found) {
                ImVec2 titleSize = ImGui::CalcTextSize(targetWp.name.c_str());
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
                ImGui::TextColored(ImVec4(targetWp.r, targetWp.g, targetWp.b, 1.0f), "%s", targetWp.name.c_str());
                ImGui::Separator();
                
                char coordBuf[64]; snprintf(coordBuf, sizeof(coordBuf), "X: %d, Y: %d, Z: %d", targetWp.x, targetWp.y, targetWp.z);
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(coordBuf).x) * 0.5f);
                ImGui::TextDisabled("%s", coordBuf);
                ImGui::Separator();
                
                if (ImGui::Selectable(LanguageManager::GetText("TELEPORT_WP"))) {
                    MapRenderState::tpTargetX = (float)targetWp.x + 0.5f;
                    MapRenderState::tpTargetY = (float)targetWp.y; 
                    MapRenderState::tpTargetZ = (float)targetWp.z + 0.5f;
                    MapRenderState::triggerTeleport.store(true);
                    MapRenderState::showBigMap = false;
                }
                
                if (ImGui::Selectable(LanguageManager::GetText("RENAME_WP"))) {
                    bigMapRenameId = selectedWpId;
                    bigMapTriggerRename = true;
                }
                
                ImGui::Separator();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Selectable(LanguageManager::GetText("DELETE_WP"))) {
                    WaypointManager::RemoveWaypoint(selectedWpId);
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);

        // 调用大地图右键地标重命名弹窗模块
        RenderRenameModal((std::string(LanguageManager::GetText("RENAME_WP")) + "##ModalBigMap").c_str(), bigMapRenameId, bigMapTriggerRename);

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    // ==========================================
    // 路径点 ImGui 管理控制台 (添加搜索、重命名与传送)
    // ==========================================
    inline const char* DimensionText(int dimensionId) {
        switch (dimensionId) {
        case 0: return LanguageManager::GetText("DIM_OVERWORLD");
        case 1: return LanguageManager::GetText("DIM_NETHER");
        case 2: return LanguageManager::GetText("DIM_END");
        default: return LanguageManager::GetText("DIM_UNKNOWN");
        }
    }

    inline std::string FormatDeathTime(long long timestamp) {
        std::time_t raw = static_cast<std::time_t>(timestamp);
        std::tm tm{};
        if (localtime_s(&tm, &raw) != 0) return "-";
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
        return buf;
    }

    inline void RenderImGuiDeathPointUI() {
        ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(LanguageManager::GetText("DEATH_POINTS_TITLE"), &MapRenderState::showDeathPointUI, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        std::vector<DeathPoint> points;
        {
            std::lock_guard<std::mutex> lock(DeathPointManager::g_deathMutex);
            points = DeathPointManager::g_deathPoints;
        }

        if (points.empty()) {
            ImGui::Dummy(ImVec2(1.0f, 120.0f));
            float textWidth = ImGui::CalcTextSize(LanguageManager::GetText("DEATH_POINTS_EMPTY")).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
            ImGui::TextUnformatted(LanguageManager::GetText("DEATH_POINTS_EMPTY"));
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("%s", LanguageManager::GetText("DEATH_POINTS_HINT"));
        ImGui::Separator();

        bool triggerTeleport = false;
        ImGui::BeginChild("DeathPointList", ImVec2(0, 0), false);
        for (const auto& point : points) {
            ImGui::PushID(point.id.c_str());

            bool sameDimension = point.dimensionId == MapRenderState::currentDimensionId;
            bool compassConnected = MapRenderState::externalCompassStatus.load() == 3;
            bool pointing = ExternalCompassSync::IsTargetPointing(point.id);

            ImVec4 rowBg = pointing ? OreColor(67, 50, 25, 230) : OreColor(35, 39, 42, 220);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, rowBg);
            ImGui::BeginChild("DeathRow", ImVec2(0, 132), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::BeginGroup();
            ImVec4 dimColor = sameDimension ? OreColor(108, 214, 140) : OreColor(170, 174, 178);
            OreTag(DimensionText(point.dimensionId), dimColor);
            if (pointing) {
                ImGui::SameLine();
                OreTag(LanguageManager::GetText("DEATH_POINT_CANCEL_COMPASS"), OreColor(234, 197, 79));
            }
            ImGui::Text("X:%d  Y:%d  Z:%d", point.x, point.y, point.z);
            ImGui::TextDisabled("%s", FormatDeathTime(point.timestamp).c_str());
            ImGui::EndGroup();

            float buttonWidth = 158.0f;
            float rightX = ImGui::GetWindowWidth() - buttonWidth - 16.0f;
            ImGui::SameLine(rightX);
            ImGui::BeginGroup();

            if (!sameDimension) ImGui::BeginDisabled();
            PushOreButtonStyle(OreButtonKind::Primary);
            if (ImGui::Button(LanguageManager::GetText("DEATH_POINT_TELEPORT"), ImVec2(buttonWidth, 0))) {
                MapRenderState::tpTargetX = (float)point.x + 0.5f;
                MapRenderState::tpTargetY = (float)point.y;
                MapRenderState::tpTargetZ = (float)point.z + 0.5f;
                MapRenderState::triggerTeleport.store(true);
                triggerTeleport = true;
            }
            PopOreButtonStyle();
            if (!sameDimension) ImGui::EndDisabled();

            if (!sameDimension || !compassConnected) ImGui::BeginDisabled();
            const char* compassLabel = pointing ? LanguageManager::GetText("DEATH_POINT_CANCEL_COMPASS") : LanguageManager::GetText("DEATH_POINT_POINT_COMPASS");
            PushOreButtonStyle(OreButtonKind::Compass);
            if (ImGui::Button(compassLabel, ImVec2(buttonWidth, 0))) {
                if (pointing) {
                    ExternalCompassSync::ClearTargetPoint();
                } else {
                    ExternalCompassSync::SetTargetPoint(point.id, (float)point.x, (float)point.z, point.dimensionId);
                }
            }
            PopOreButtonStyle();
            if (!sameDimension || !compassConnected) ImGui::EndDisabled();

            PushOreButtonStyle(OreButtonKind::Danger);
            if (ImGui::Button(LanguageManager::GetText("DEATH_POINT_DELETE"), ImVec2(buttonWidth, 0))) {
                if (pointing) ExternalCompassSync::ClearTargetPoint();
                DeathPointManager::RemoveDeathPoint(point.id);
            }
            PopOreButtonStyle();
            ImGui::EndGroup();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PopID();
        }
        ImGui::EndChild();

        // 传送命令由键盘模拟输入；窗口仍打开时会拦截回车，必须先退出所有地图 UI。
        if (triggerTeleport) {
            MapRenderState::showDeathPointUI = false;
            MapRenderState::showBigMap = false;
        }

        ImGui::End();
    }

    inline void RenderImGuiWaypointUI() {
        ImGui::SetNextWindowSize(ImVec2(750, 480), ImGuiCond_FirstUseEver); 
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2 - 375, ImGui::GetIO().DisplaySize.y / 2 - 240), ImGuiCond_FirstUseEver);
        
        ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin(LanguageManager::GetText("WP_MANAGER_TITLE"), &MapRenderState::showWaypointUI, winFlags)) {
            
            static bool showAddPopup = false;
            static char searchBuf[256] = "";
            
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
            ImGui::InputTextWithHint("##WPSearch", LanguageManager::GetText("SEARCH_HINT"), searchBuf, sizeof(searchBuf));
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("NEW_WP_BUTTON"), ImVec2(140, 0))) {
                showAddPopup = true;
            }
            ImGui::Separator();

            ImGui::BeginChild("WPList", ImVec2(0, 0), true);
            std::string toDelete = "";
            bool toggled = false;
            bool triggerTp = false; 

            static std::string uiRenameId = "";
            static bool uiTriggerRename = false;

            std::string query = searchBuf;
            for (char& c : query) { if (c >= 'A' && c <= 'Z') c += 32; }

            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for (auto& wp : WaypointManager::g_waypoints) {
                    
                    if (!query.empty()) {
                        std::string lowerName = wp.name;
                        for (char& c : lowerName) { if (c >= 'A' && c <= 'Z') c += 32; }
                        if (lowerName.find(query) == std::string::npos) {
                            continue; 
                        }
                    }

                    ImGui::PushID(wp.id.c_str());
                    
                    ImGui::ColorButton("##color", ImVec4(wp.r, wp.g, wp.b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24));
                    ImGui::SameLine();
                    
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                    ImGui::Text("%s", wp.name.c_str());
                    
                    float winWidth = ImGui::GetWindowWidth();
                    
                    ImGui::SameLine(winWidth > 750 ? winWidth - 490 : 200);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "X: %d  Y: %d  Z: %d", wp.x, wp.y, wp.z);
                    
                    ImGui::SameLine(winWidth - 270);
                    bool enabled = wp.enabled;
                    if (ImGui::Checkbox(LanguageManager::GetText("WP_LIST_SHOW"), &enabled)) {
                        wp.enabled = enabled;
                        toggled = true;
                    }
                    
                    ImGui::SameLine(winWidth - 195);
                    PushOreButtonStyle(OreButtonKind::Warning);
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_RENAME"), ImVec2(55, 0))) {
                        uiRenameId = wp.id;
                        uiTriggerRename = true;
                    }
                    PopOreButtonStyle();

                    ImGui::SameLine(winWidth - 135);
                    PushOreButtonStyle(OreButtonKind::Primary);
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_TELEPORT"), ImVec2(45, 0))) {
                        MapRenderState::tpTargetX = (float)wp.x + 0.5f;
                        MapRenderState::tpTargetY = (float)wp.y; 
                        MapRenderState::tpTargetZ = (float)wp.z + 0.5f;
                        MapRenderState::triggerTeleport.store(true);
                        triggerTp = true;
                    }
                    PopOreButtonStyle();

                    ImGui::SameLine(winWidth - 75);
                    PushOreButtonStyle(OreButtonKind::Danger);
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_DELETE"), ImVec2(45, 0))) {
                        toDelete = wp.id;
                    }
                    PopOreButtonStyle();
                    
                    ImGui::PopID();
                    ImGui::Separator();
                }
            } 

            if (!toDelete.empty()) {
                WaypointManager::RemoveWaypoint(toDelete);
                WaypointManager::SaveWaypoints();
            } else if (toggled) {
                WaypointManager::SaveWaypoints();
            }
            
            if (triggerTp) {
                MapRenderState::showWaypointUI = false;
                MapRenderState::showBigMap = false;
            }
            
            ImGui::EndChild();

            // 调用 UI 列表专属重命名弹窗模块
            RenderRenameModal((std::string(LanguageManager::GetText("RENAME_WP")) + "##ModalUI").c_str(), uiRenameId, uiTriggerRename);

            if (MapRenderState::triggerAddWaypoint) {
                showAddPopup = true;
                MapRenderState::triggerAddWaypoint = false;
            }

            if (showAddPopup) ImGui::OpenPopup(LanguageManager::GetText("NEW_WP_TITLE"));
            
            if (ImGui::BeginPopupModal(LanguageManager::GetText("NEW_WP_TITLE"), &showAddPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                static char nameBuf[256] = "";
                static int pos[3] = {0, 0, 0};
                static float col[3] = {1.0f, 0.3f, 0.3f};
                
                if (ImGui::IsWindowAppearing()) {
                    nameBuf[0] = '\0'; 
                    pos[0] = (MapRenderState::addWaypointX != -999999) ? MapRenderState::addWaypointX : g_playerBlockX;
                    pos[1] = (MapRenderState::addWaypointY != -999999) ? MapRenderState::addWaypointY : (int)g_playerY;
                    pos[2] = (MapRenderState::addWaypointZ != -999999) ? MapRenderState::addWaypointZ : g_playerBlockZ;
                    
                    MapRenderState::addWaypointX = -999999;
                    MapRenderState::addWaypointY = -999999;
                    MapRenderState::addWaypointZ = -999999;
                }

                ImGui::InputText(LanguageManager::GetText("WP_NAME"), nameBuf, sizeof(nameBuf));
                ImGui::InputInt3("X / Y / Z", pos);
                ImGui::ColorEdit3(LanguageManager::GetText("WP_COLOR"), col);
                
                ImGui::Spacing();
                PushOreButtonStyle(OreButtonKind::Success);
                if (ImGui::Button(LanguageManager::GetText("WP_SAVE"), ImVec2(120, 0))) {
                    WaypointManager::AddWaypoint(nameBuf, pos[0], pos[1], pos[2], col[0], col[1], col[2]);
                    showAddPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                PopOreButtonStyle();
                ImGui::SameLine();
                PushOreButtonStyle(OreButtonKind::Default);
                if (ImGui::Button(LanguageManager::GetText("WP_CANCEL"), ImVec2(120, 0))) {
                    showAddPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                PopOreButtonStyle();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    inline void RenderPositionSettingsPanel() {
        if (!MapRenderState::showPositionSettings) return;

        ImGuiIO& io = ImGui::GetIO();
        float mmr = MapRenderState::minimapSize;
        float sx = io.DisplaySize.x;
        float sy = io.DisplaySize.y;
        float maxX = std::max(0.0f, sx * 0.5f - mmr);
        float maxY = std::max(0.0f, sy * 0.5f - mmr);
        MapRenderState::tempMinimapOffsetX = std::clamp(MapRenderState::tempMinimapOffsetX, -maxX, maxX);
        MapRenderState::tempMinimapOffsetY = std::clamp(MapRenderState::tempMinimapOffsetY, -maxY, maxY);

        ImGui::SetNextWindowPos(ImVec2(sx * 0.5f, sy * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360, 220));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGuiWindowFlags posFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
        bool open = true;
        if (ImGui::Begin("PositionSettings", &open, posFlags)) {
            OreSectionHeader(LanguageManager::GetText("POSITION_SETTINGS"));
            ImGui::Spacing();

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat(LanguageManager::GetText("POSITION_X"), &MapRenderState::tempMinimapOffsetX, -maxX, maxX, "%.0f");
            ImGui::SliderFloat(LanguageManager::GetText("POSITION_Y"), &MapRenderState::tempMinimapOffsetY, -maxY, maxY, "%.0f");
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float btnW = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;
            PushOreButtonStyle(OreButtonKind::Success);
            if (ImGui::Button(LanguageManager::GetText("SAVE_EXIT"), ImVec2(btnW, 35.0f))) {
                MapRenderState::minimapOffsetX = MapRenderState::tempMinimapOffsetX;
                MapRenderState::minimapOffsetY = MapRenderState::tempMinimapOffsetY;
                LanguageManager::SaveConfig();
                MapRenderState::showPositionSettings = false;
                MapRenderState::showBigMap = true;
            }
            PopOreButtonStyle();
            ImGui::SameLine();
            PushOreButtonStyle(OreButtonKind::Default);
            if (ImGui::Button(LanguageManager::GetText("DONT_SAVE"), ImVec2(btnW, 35.0f))) {
                MapRenderState::tempMinimapOffsetX = MapRenderState::minimapOffsetX;
                MapRenderState::tempMinimapOffsetY = MapRenderState::minimapOffsetY;
                MapRenderState::showPositionSettings = false;
                MapRenderState::showBigMap = true;
            }
            PopOreButtonStyle();
        }
        ImGui::End();
    }

    inline void RenderFrameToRTV(ID3D11RenderTargetView* rtv, bool releaseRtv) {
        auto readKey = [](int vk) -> SHORT {
            return oGetAsyncKeyState ? oGetAsyncKeyState(vk) : GetAsyncKeyState(vk);
        };
        // M 键由 BRD 转发的 WndProc 消息处理；这里仅保留 Tab 轮询兜底。
        // 否则 M 会在 WndProc 与轮询中各触发一次，导致切换后立即抵消。
        g_tabHeld = (readKey(VK_TAB) & 0x8000) != 0;

        g_pd3dDeviceContext->OMSetRenderTargets(1, &rtv, NULL);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (g_hWnd) {
            RECT rc{};
            if (GetClientRect(g_hWnd, &rc)) {
                ImGui::GetIO().DisplaySize = ImVec2((float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
            }
        }
        ImGui::NewFrame();
        ImGui::GetIO().MouseDrawCursor = MapRenderState::IsUIActive();

        UpdateSmoothCamera();

        if (MapRenderState::showBigMap) {
            RenderImGuiBigMap();
        } else {
            RenderImGuiXaeroMap();
        }

        if (MapRenderState::showWaypointUI) {
            RenderImGuiWaypointUI();
        }

        if (MapRenderState::showDeathPointUI) {
            RenderImGuiDeathPointUI();
        }

        RenderPositionSettingsPanel();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        ID3D11RenderTargetView* nullRTV = nullptr;
        g_pd3dDeviceContext->OMSetRenderTargets(1, &nullRTV, NULL);
        if (releaseRtv) rtv->Release();
    }

    inline void RenderFromBRD(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv) {
        static bool loggedEntry = false;
        static bool loggedNoPlayer = false;
        static bool loggedInit = false;
        static bool loggedDraw = false;
        if (!loggedEntry) {
            DbgLog("RenderFromBRD entered");
            loggedEntry = true;
        }
        if (!device || !context || !rtv) {
            DbgLog("RenderFromBRD missing device/context/rtv");
            return;
        }
        if (!ShouldRenderMapOverlay()) {
            if (!loggedNoPlayer) {
                DbgLog("RenderFromBRD waiting for player");
                loggedNoPlayer = true;
            }
            return;
        }
        static bool initAttempted = false;
        if (!g_imguiInitialized && !initAttempted) {
            initAttempted = true;
            if (!g_hWnd) g_hWnd = FindMinecraftWindow();
            if (!g_hWnd) return;
            g_pd3dDevice = device;
            g_pd3dDeviceContext = context;
            g_pd3dDevice->AddRef();
            g_pd3dDeviceContext->AddRef();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            InitImGuiFonts(io);
            ApplyOreUIStyle();
            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
            InitMapTexture();
            g_imguiInitialized = true;
            if (!loggedInit) {
                DbgLog("RenderFromBRD initialized ImGui");
                loggedInit = true;
            }
        }
        if (g_imguiInitialized) {
            if (!loggedDraw) {
                DbgLog("RenderFromBRD drawing map");
                loggedDraw = true;
            }
            RenderFrameToRTV(rtv, false);
        }
    }

    inline void RenderImGui(IDXGISwapChain* pSwapChain) {
        static std::atomic<bool> isRendering{false};
        if (isRendering.exchange(true)) return;

        if (!ShouldRenderMapOverlay()) {
            isRendering = false;
            return;
        }

        static bool initAttempted = false; 
        if (!g_imguiInitialized && !initAttempted) {
            HRESULT hr11 = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
            if (SUCCEEDED(hr11)) {
                initAttempted = true;
                g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            } else {
                if (!g_pGameCommandQueue) { isRendering = false; return; }
                initAttempted = true;
                ID3D12Device* pD3D12Device = nullptr;
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pD3D12Device))) {
                    if (SUCCEEDED(D3D11On12CreateDevice(pD3D12Device, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, (IUnknown**)&g_pGameCommandQueue, 1, 0, &g_pd3dDevice, &g_pd3dDeviceContext, nullptr))) {
                        g_pd3dDevice->QueryInterface(__uuidof(ID3D11On12Device), (void**)&g_d3d11On12Device);
                    }
                    pD3D12Device->Release();
                }
            }

            if (g_pd3dDevice) {
                DXGI_SWAP_CHAIN_DESC sd;
                pSwapChain->GetDesc(&sd);
                g_hWnd = sd.OutputWindow;
                if (!g_hWnd) g_hWnd = FindMinecraftWindow();

                oWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();

                InitImGuiFonts(io);
                ApplyOreUIStyle();
                ImGui_ImplWin32_Init(g_hWnd);
                ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
                
                InitMapTexture(); 
                g_imguiInitialized = true;
                g_brdPresent = (GetModuleHandleA("BetterRenderDragon.dll") != nullptr);
            }
        }

        if (g_imguiInitialized && g_hasPlayer) {
            auto renderImGuiFrame = [&](ID3D11RenderTargetView* rtv) {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &rtv, NULL);
                ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
                ImGui::GetIO().MouseDrawCursor = MapRenderState::IsUIActive();

                UpdateSmoothCamera(); // 更新底层的无极平滑推测坐标！

                if (MapRenderState::showBigMap) {
                    RenderImGuiBigMap();
                } else {
                    RenderImGuiXaeroMap();
                }

                if (MapRenderState::showWaypointUI) {
                    RenderImGuiWaypointUI();
                }

                if (MapRenderState::showDeathPointUI) {
                    RenderImGuiDeathPointUI();
                }

                RenderPositionSettingsPanel();

                ImGui::Render();
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                ID3D11RenderTargetView* nullRTV = nullptr;
                g_pd3dDeviceContext->OMSetRenderTargets(1, &nullRTV, NULL);
                rtv->Release();
            };

            if (g_d3d11On12Device) {
                UINT bufferIndex = 0;
                IDXGISwapChain3* pSwapChain3 = nullptr;
                if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pSwapChain3))) {
                    bufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
                    pSwapChain3->Release();
                }

                ID3D12Resource* d3d12BackBuffer = nullptr;
                if (SUCCEEDED(pSwapChain->GetBuffer(bufferIndex, __uuidof(ID3D12Resource), (void**)&d3d12BackBuffer))) {
                    ID3D11Resource* wrappedBackBuffer = nullptr;
                    D3D11_RESOURCE_FLAGS d3d11Flags = {D3D11_BIND_RENDER_TARGET};

                    if (SUCCEEDED(g_d3d11On12Device->CreateWrappedResource(
                        d3d12BackBuffer, &d3d11Flags,
                        D3D12_RESOURCE_STATE_RENDER_TARGET,
                        g_brdPresent ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PRESENT,
                        __uuidof(ID3D11Resource), (void**)&wrappedBackBuffer)))
                    {
                        ID3D11RenderTargetView* rtv = nullptr;
                        g_pd3dDevice->CreateRenderTargetView(wrappedBackBuffer, NULL, &rtv);
                        g_d3d11On12Device->AcquireWrappedResources(&wrappedBackBuffer, 1);

                        if (rtv) renderImGuiFrame(rtv);

                        g_d3d11On12Device->ReleaseWrappedResources(&wrappedBackBuffer, 1);
                        wrappedBackBuffer->Release();

                        g_pd3dDeviceContext->ClearState();
                        g_pd3dDeviceContext->Flush();
                    }
                    d3d12BackBuffer->Release();
                }
            } else {
                ID3D11Texture2D* pBackBuffer = nullptr;
                if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
                    ID3D11RenderTargetView* rtv = nullptr;
                    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &rtv);
                    pBackBuffer->Release();
                    if (rtv) {
                        renderImGuiFrame(rtv);
                        g_pd3dDeviceContext->ClearState();
                        g_pd3dDeviceContext->Flush();
                    }
                }
            }
        }
        isRendering = false;
    }

    inline HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        RenderImGui(pSwapChain);
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    inline HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams) {
        RenderImGui(pSwapChain);
        return oPresent1(pSwapChain, SyncInterval, Flags, pParams);
    }

    inline bool init() {
        HWND hwnd = FindMinecraftWindow();
        if (!hwnd) return false;
        g_brdPresent = (GetModuleHandleA("BetterRenderDragon.dll") != nullptr);
        
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* dummyDevice = nullptr;
        IDXGISwapChain* dummySwapChain = nullptr;
        ID3D11DeviceContext* dummyContext = nullptr;

        MH_STATUS status = MH_Initialize();

        // BRD 共存时由 BRD 的 Present/ImGui 管线调用 ChiyanMap_RenderFromBRD，
        // ChiyanMap 不再抢 Present/Resize/ExecuteCommandLists，避免双方图形钩子互相覆盖。
        if (g_brdPresent) {
            if (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED) {
                g_hWnd = hwnd;
                HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
                if (hUser32) {
                    auto hookUser32 = [&](LPCSTR name, LPVOID detour, LPVOID* original) {
                        void* fn = (void*)GetProcAddress(hUser32, name);
                        if (fn && MH_CreateHook(fn, detour, original) == MH_OK) MH_EnableHook(fn);
                    };
                    hookUser32("GetRawInputData", (LPVOID)hkGetRawInputData, (LPVOID*)&oGetRawInputData);
                    hookUser32("GetRawInputBuffer", (LPVOID)hkGetRawInputBuffer, (LPVOID*)&oGetRawInputBuffer);
                    hookUser32("GetAsyncKeyState", (LPVOID)hkGetAsyncKeyState, (LPVOID*)&oGetAsyncKeyState);
                    hookUser32("GetKeyState", (LPVOID)hkGetKeyState, (LPVOID*)&oGetKeyState);
                    hookUser32("GetCursorPos", (LPVOID)hkGetCursorPos, (LPVOID*)&oGetCursorPos);
                    hookUser32("SetCursorPos", (LPVOID)hkSetCursorPos, (LPVOID*)&oSetCursorPos);
                }
                return true;
            }
            return false;
        }

        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1, D3D11_SDK_VERSION, &sd, &dummySwapChain, &dummyDevice, NULL, &dummyContext))) {
            void** pVTable = *reinterpret_cast<void***>(dummySwapChain);
            if (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED) {
                MH_CreateHook(pVTable[8], (LPVOID)hkPresent, (void**)&oPresent);
                MH_EnableHook(pVTable[8]);

                MH_CreateHook(pVTable[13], (LPVOID)hkResizeBuffers, (void**)&oResizeBuffers);
                MH_EnableHook(pVTable[13]);

                IDXGISwapChain1* dummySwapChain1 = nullptr;
                if (SUCCEEDED(dummySwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&dummySwapChain1))) {
                    void** pVTable1 = *reinterpret_cast<void***>(dummySwapChain1);
                    MH_CreateHook(pVTable1[22], (LPVOID)hkPresent1, (void**)&oPresent1);
                    MH_EnableHook(pVTable1[22]);
                    dummySwapChain1->Release();
                }

                HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
                if (hUser32) {
                    void* pGetRawInputData = (void*)GetProcAddress(hUser32, "GetRawInputData");
                    if (pGetRawInputData && MH_CreateHook(pGetRawInputData, (LPVOID)hkGetRawInputData, (void**)&oGetRawInputData) == MH_OK) {
                        MH_EnableHook(pGetRawInputData);
                    }
                    void* pGetRawInputBuffer = (void*)GetProcAddress(hUser32, "GetRawInputBuffer");
                    if (pGetRawInputBuffer && MH_CreateHook(pGetRawInputBuffer, (LPVOID)hkGetRawInputBuffer, (void**)&oGetRawInputBuffer) == MH_OK) {
                        MH_EnableHook(pGetRawInputBuffer);
                    }
                    void* pGetAsyncKeyState = (void*)GetProcAddress(hUser32, "GetAsyncKeyState");
                    if (pGetAsyncKeyState && MH_CreateHook(pGetAsyncKeyState, (LPVOID)hkGetAsyncKeyState, (void**)&oGetAsyncKeyState) == MH_OK) {
                        MH_EnableHook(pGetAsyncKeyState);
                    }
                    void* pGetKeyState = (void*)GetProcAddress(hUser32, "GetKeyState");
                    if (pGetKeyState && MH_CreateHook(pGetKeyState, (LPVOID)hkGetKeyState, (void**)&oGetKeyState) == MH_OK) {
                        MH_EnableHook(pGetKeyState);
                    }
                    void* pGetCursorPos = (void*)GetProcAddress(hUser32, "GetCursorPos");
                    if (pGetCursorPos && MH_CreateHook(pGetCursorPos, (LPVOID)hkGetCursorPos, (void**)&oGetCursorPos) == MH_OK) {
                        MH_EnableHook(pGetCursorPos);
                    }
                    void* pSetCursorPos = (void*)GetProcAddress(hUser32, "SetCursorPos");
                    if (pSetCursorPos && MH_CreateHook(pSetCursorPos, (LPVOID)hkSetCursorPos, (void**)&oSetCursorPos) == MH_OK) {
                        MH_EnableHook(pSetCursorPos);
                    }
                }
            }
            dummySwapChain->Release(); dummyDevice->Release(); dummyContext->Release();
        }

        // BRD 自己会先探测/挂钩图形后端；ChiyanMap 在 BRD 共存时不再创建 dummy D3D12 设备，
        // 避免把 BRD 从 D3D11 路径推到 D3D12 路径，导致 BRD 的 clientInstance_Update 签名失配。
        ID3D12Device* pDummyD12Device = nullptr;
        if (!g_brdPresent && SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDummyD12Device))) {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            ID3D12CommandQueue* pDummyQueue = nullptr;
            if (SUCCEEDED(pDummyD12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pDummyQueue))) {
                void** pVTable12 = *reinterpret_cast<void***>(pDummyQueue);
                if (MH_CreateHook(pVTable12[10], (LPVOID)hkExecuteCommandLists, (void**)&oExecuteCommandLists) == MH_OK) {
                    MH_EnableHook(pVTable12[10]);
                }
                pDummyQueue->Release();
            }
            pDummyD12Device->Release();
        }
        return true;
    }
}
