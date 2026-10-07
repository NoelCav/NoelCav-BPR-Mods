#pragma once


#include <atomic>
#include <mutex>

#include "core/Path.hpp"
#include "core/Logger.hpp"
#include "core/Pointer.hpp"

#include "TeleportLocationsFile.hpp"


class Teleport
{
private:
    Teleport();

public:
    static Teleport& Get();

public:
    void Load();
    void Unload();

private:
    void OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue);

    void UpdateCurrentTransform();
    bool GetCurrentTransform(float position[3], float direction[3]);

    void RenderMenu();

    void OnGameMain();
    void RenderMapPrompts();

    void LoadConfig();
    void SaveConfig() const;

    void RequestTeleport(const float position[3], const float direction[3]);

    void InstallXInputHook();
    void RemoveXInputHook();
    static unsigned long __stdcall Hook_XInputGetState(unsigned long userIndex, void* state);

private:
    static constexpr char k_Name[] = "Teleport";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "NoelCav";
    static constexpr char k_ConfigDirectoryPath[] = "teleport\\";

    static Teleport s_Instance;

private:
    Core::Logger m_Logger;

    Core::Path m_ConfigDirectoryPath;
    Core::Path m_ConfigFilePath;

    TeleportLocationsFile m_TeleportLocationsFile;

    std::atomic<bool> m_TeleportPending = false;
    // Frames the driving HUD has been up; pending teleports wait for it (see OnGameStatePreWorldUpdate).
    int m_DrivingFrames = 0;
    std::atomic<bool> m_Driving = false;
    std::atomic<bool> m_InJunkyard = false;
    float m_PendingPosition[3] = {};
    float m_PendingDirection[3] = {};

    std::mutex m_CurrentTransformMutex;
    bool m_CurrentTransformValid = false;
    float m_CurrentPosition[3] = {};
    float m_CurrentDirection[3] = {};

    float m_InputPosition[3] = {};
    float m_InputDirection[3] = { 1.0f, 0.0f, 0.0f };
    char m_NewLocationName[64] = {};

    // A teleport asked for while paused closes the pause menu by pressing B for a few frames
    // (or Esc without a controller); the teleport then happens once back on the road.
    std::atomic<bool> m_CloseMenuRequested = false;
    std::atomic<int> m_PressBackFrames = 0;
    std::atomic<bool> m_ControllerConnected = false;
    bool m_HookInstalled = false;

    // Map hotkeys (can be turned off): A / keyboard 2 teleports outside the Junkyard under the map
    // cursor, X / Enter into it. Our own button prompts are drawn next to the map's, for the device used last.
    uint8_t m_MapKeysDown = 0;
    uint16_t m_MapButtonsDown = 0;
    std::atomic<bool> m_UsingKeyboard = false;
    std::atomic<bool> m_MapTeleport = true;
    int m_PromptIcons = 0; // 0 = follow the last used device, 1 = controller, 2 = keyboard
    std::atomic<uint32_t> m_HoveredMapItem = 0;

    std::atomic<void*> m_PromptFont = nullptr;    // ImFont*, loaded once in game by a helper thread
    std::atomic<void*> m_ButtonTexture = nullptr; // ID3D11ShaderResourceView* of the game's button sheet
    int m_MenuFrame = -10;
    // Measured from the map screen: our prompts end one prompt-gap left of the game's "Y Zoom Out".
    float m_PromptX = 260.0f;     // right edge of the prompts, from the screen center (1080p pixels)
    float m_PromptY = 149.0f;     // prompt row center, up from the bottom of the screen
    float m_PromptScale = 1.0f;
    float m_PromptIconScale = 1.3f; // icon size relative to the text
};
