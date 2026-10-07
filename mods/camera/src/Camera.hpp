#pragma once


#include <atomic>
#include <cstdint>

#include "core/Path.hpp"
#include "core/Logger.hpp"
#include "core/Pointer.hpp"


// Custom chase camera: moves the default driving camera up/back, tilts it and widens it, as
// offsets on top of each car's own camera settings. Still follows the car exactly like the
// stock camera; only its tuning changes.
class Camera
{
private:
    Camera();

public:
    static Camera& Get();

public:
    void Load();
    void Unload();

private:
    void OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue);

    void LoadConfig();
    void SaveConfig() const;

    void RenderMenu();

private:
    static constexpr char k_Name[] = "Camera";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "NoelCav";
    static constexpr char k_ConfigDirectoryPath[] = "camera\\";

    static Camera s_Instance;

    enum Setting
    {
        Setting_Height,
        Setting_Distance,
        Setting_DownAngle,
        Setting_FieldOfView,
        Setting_Count,
    };

private:
    Core::Logger m_Logger;

    Core::Path m_ConfigDirectoryPath;
    Core::Path m_ConfigFilePath;

    // Menu -> game thread.
    std::atomic<bool> m_Enabled = true;
    std::atomic<float> m_Offsets[Setting_Count] = {};

    // Game thread only.
    uint64_t m_AttribKey = 0;
    bool m_HaveDefaults = false;
    float m_Defaults[Setting_Count] = {};
    float m_Written[Setting_Count] = {};

    // Game thread -> menu, for display.
    std::atomic<float> m_DisplayDefaults[Setting_Count] = {};
};
