#pragma once


#include <atomic>

#include "core/Path.hpp"
#include "core/Logger.hpp"


class Controls
{
private:
    Controls();

public:
    static Controls& Get();

public:
    void Load();
    void Unload();

private:
    void InstallXInputHook();
    void RemoveXInputHook();

    void LoadConfig();
    void SaveConfig() const;

    void RenderMenu();

    static bool IsInShowtime();
    static unsigned long __stdcall Hook_XInputGetState(unsigned long userIndex, void* state);

private:
    static constexpr char k_Name[] = "Controls";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "NoelCav";
    static constexpr char k_ConfigDirectoryPath[] = "controls\\";

    static Controls s_Instance;

private:
    Core::Logger m_Logger;

    Core::Path m_ConfigDirectoryPath;
    Core::Path m_ConfigFilePath;

    bool m_HookInstalled = false;

    // Read from the XInput hook on the game's input thread, written from the menu.
    std::atomic<bool> m_InvertLookVertical = true;
    std::atomic<bool> m_InvertLookHorizontalDriving = true;
    std::atomic<bool> m_InvertLookHorizontalShowtime = false;
};
