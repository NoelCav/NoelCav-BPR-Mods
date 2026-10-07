#pragma once


#include <atomic>

#include "core/Path.hpp"
#include "core/Logger.hpp"


// Borderless windowed fix: the game sizes its borderless window to the monitor's work area but
// still renders at its configured resolution, which leaves the taskbar visible and squashes the
// picture (and misplaces overlays and clicks). This stretches the window over the whole monitor.
class Borderless
{
private:
    Borderless();

public:
    static Borderless& Get();

public:
    void Load();
    void Unload();

private:
    void OnGameMain();

    void LoadConfig();
    void SaveConfig() const;

    void RenderMenu();

    static long __stdcall WindowProc(void* window, unsigned int message, unsigned int wParam, long lParam);

private:
    static constexpr char k_Name[] = "Borderless";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "NoelCav";
    static constexpr char k_ConfigDirectoryPath[] = "borderless\\";

    static Borderless s_Instance;

private:
    Core::Logger m_Logger;

    Core::Path m_ConfigDirectoryPath;
    Core::Path m_ConfigFilePath;

    std::atomic<bool> m_Enabled = true;

    // Game thread only.
    void* m_Window = nullptr;           // HWND, once subclassed
    void* m_OriginalWindowProc = nullptr;
    std::atomic<bool> m_RefitPending = false; // menu -> game thread
};
