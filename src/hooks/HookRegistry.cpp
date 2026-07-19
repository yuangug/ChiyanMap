#include "hooks/HookRegistry.h"
#include "hooks/DX11Hook.h"
#include "hooks/PlayerHook.h"
#include "hooks/UIRenderHook.h"

extern "C" __declspec(dllexport) void ChiyanMap_RenderFromBRD(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* rtv
) {
    DX11Hook::RenderFromBRD(device, context, rtv);
}

extern "C" __declspec(dllexport) LRESULT ChiyanMap_WndProcFromBRD(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    return DX11Hook::HandleWndProcFromBRD(hWnd, uMsg, wParam, lParam);
}

extern "C" __declspec(dllexport) bool ChiyanMap_IsUIActive() {
    return MapRenderState::IsUIActive();
}

void registerAllHooks() {
    DX11Hook::init();
    // 只注册原生的游戏逻辑与 UI 钩子
    if (!RegisterBRDClientInstanceUpdateCallback()) {
        ClientInstanceUpdateHook::hook();
    }
    UIRenderContextFlushTextHook::hook();

    LocalPlayerApplyTurnDeltaHook::hook();
    GameModeStartDestroyBlockHook::hook();
    GameModeUseItemHook::hook();
    GameModeAttackHook::hook();

    ActorIsImmobileHook::hook();
    LocalPlayerSwingHook::hook();
    GameModeUseItemOnHook::hook();
    GameModeContinueDestroyBlockHook::hook();

    MobSetCarriedItemHook::hook();
    GameModeBaseUseItemHook::hook();

    PlayerInventorySelectSlotHook::hook();
    PlayerInventorySetItemHook::hook();
    LocalPlayerPickBlockHook::hook();
    LocalPlayerJumpFromGroundHook::hook();
    LocalPlayerSetSneakingHook::hook();
    LocalPlayerIsImmobileHook::hook();
}

void unregisterAllHooks() {
    if (g_registeredBRDClientInstanceCallback) {
        UnregisterBRDClientInstanceUpdateCallback();
    } else {
        ClientInstanceUpdateHook::unhook();
    }
    UIRenderContextFlushTextHook::unhook();

    LocalPlayerApplyTurnDeltaHook::unhook();
    GameModeStartDestroyBlockHook::unhook();
    GameModeUseItemHook::unhook();
    GameModeAttackHook::unhook();

    ActorIsImmobileHook::unhook();
    LocalPlayerSwingHook::unhook();
    GameModeUseItemOnHook::unhook();
    GameModeContinueDestroyBlockHook::unhook();

    MobSetCarriedItemHook::unhook();
    GameModeBaseUseItemHook::unhook();

    PlayerInventorySelectSlotHook::unhook();
    PlayerInventorySetItemHook::unhook();
    LocalPlayerPickBlockHook::unhook();
    LocalPlayerJumpFromGroundHook::unhook();
    LocalPlayerSetSneakingHook::unhook();
    LocalPlayerIsImmobileHook::unhook();
}
