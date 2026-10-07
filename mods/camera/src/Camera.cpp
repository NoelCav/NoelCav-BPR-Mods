#include "Camera.hpp"

#include <exception>

#include <Windows.h>

#include "vendor/imgui.hpp"
#include "vendor/yaml-cpp.hpp"

#include "core/File.hpp"
#include "mod-manager/ModManager.hpp"


namespace
{
    // BrnDirector::Camera::BehaviourParameterBank, at gm+0x714140. +0x2480 is the attrib key of
    // the loaded camera settings (changes with the car), +0x2488 the gameplay external (chase)
    // camera parameters. Offsets from matty-ross's free-camera mod (bpr-mods-repository).
    constexpr ptrdiff_t k_ParameterBank = 0x714140;
    constexpr ptrdiff_t k_AttribKey = 0x2480;
    constexpr ptrdiff_t k_Parameters = 0x2488;

    struct SettingInfo
    {
        const char* Label;
        const char* ConfigKey;
        ptrdiff_t Offset; // into the chase camera parameters
        float Min;
        float Max;
    };

    constexpr SettingInfo k_Settings[] =
    {
        { "Height",        "Height",      0x4C, -3.0f,  10.0f }, // PivotY
        { "Distance",      "Distance",    0x50, -3.0f,  15.0f }, // PivotZ
        { "Down angle",    "DownAngle",   0x94, -20.0f, 30.0f }, // DownAngle, degrees
        { "Field of view", "FieldOfView", 0x6C, -30.0f, 40.0f }, // FOV, degrees
    };

    Core::Pointer GetParameterBank()
    {
        Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref();
        return gameModule.GetPointer() != nullptr ? gameModule.at(k_ParameterBank) : nullptr;
    }
}


Camera Camera::s_Instance;


Camera::Camera()
    :
    m_Logger(k_Name),
    m_ConfigDirectoryPath(ModManager::Get().GetConfigDirectoryPath().Append(k_ConfigDirectoryPath)),
    m_ConfigFilePath(Core::Path(m_ConfigDirectoryPath).Append("camera-config.yaml"))
{
}

Camera& Camera::Get()
{
    return s_Instance;
}

void Camera::Load()
{
    try
    {
        if (!ModManager::Get().CheckVersion(k_Version))
        {
            throw std::exception("Mod Manager and Mod versions mismatch.");
        }

        if (!m_ConfigDirectoryPath.Exists())
        {
            m_ConfigDirectoryPath.CreateDirectoryTree();
            m_Logger.Info("Created config directory. path: '%s'", m_ConfigDirectoryPath.GetPath());
        }

        LoadConfig();

        ModManager::Get().GetHookManager().AddGameStatePreWorldUpdateHook(
            [](Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
            {
                s_Instance.OnGameStatePreWorldUpdate(gameEventQueue, gameActionQueue);
            }
        );

        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Camera::Unload()
{
    SaveConfig();
}

void Camera::OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
{
    Core::Pointer bank = GetParameterBank();
    if (bank.GetPointer() == nullptr)
    {
        return;
    }

    Core::Pointer parameters = bank.at(k_Parameters);

    // New camera settings loaded (new car): take the game's values as the defaults.
    uint64_t attribKey = bank.at(k_AttribKey).as<uint64_t>();
    if (!m_HaveDefaults || attribKey != m_AttribKey)
    {
        for (int i = 0; i < Setting_Count; ++i)
        {
            m_Defaults[i] = m_Written[i] = parameters.at(k_Settings[i].Offset).as<float>();
        }
        m_AttribKey = attribKey;
        m_HaveDefaults = true;
    }

    bool enabled = m_Enabled.load();
    for (int i = 0; i < Setting_Count; ++i)
    {
        float& value = parameters.at(k_Settings[i].Offset).as<float>();

        // If the game rewrote a value itself, that's its new default.
        if (value != m_Written[i])
        {
            m_Defaults[i] = value;
        }

        value = enabled ? m_Defaults[i] + m_Offsets[i].load() : m_Defaults[i];
        m_Written[i] = value;
        m_DisplayDefaults[i] = m_Defaults[i];
    }
}

void Camera::LoadConfig()
{
    try
    {
        YAML::Node yaml = YAML::Load(
            Core::File(m_ConfigFilePath, Core::File::Mode::Read, m_Logger).ReadAsText()
        );

        m_Enabled = yaml["Enabled"].as<bool>(m_Enabled.load());
        for (int i = 0; i < Setting_Count; ++i)
        {
            m_Offsets[i] = yaml[k_Settings[i].ConfigKey].as<float>(0.0f);
        }

        m_Logger.Info("Loaded camera config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load camera config, using defaults. exception: %s", ex.what());
    }
}

void Camera::SaveConfig() const
{
    try
    {
        YAML::Node yaml;
        yaml["Enabled"] = m_Enabled.load();
        for (int i = 0; i < Setting_Count; ++i)
        {
            yaml[k_Settings[i].ConfigKey] = m_Offsets[i].load();
        }

        Core::File(m_ConfigFilePath, Core::File::Mode::Write, m_Logger).WriteAsText(
            YAML::Dump(yaml)
        );

        m_Logger.Info("Saved camera config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to save camera config. exception: %s", ex.what());
    }
}

void Camera::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(1320.0f, 920.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 330.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);

        bool changed = false;

        bool enabled = m_Enabled.load();
        if (ImGui::Checkbox("Custom chase camera", &enabled))
        {
            m_Enabled = enabled;
            changed = true;
        }

        // Shown as the camera's actual values; what's saved is the change from each car's default,
        // so the same tweak carries over to every car.
        ImGui::SeparatorText("Chase camera (this car)");
        for (int i = 0; i < Setting_Count; ++i)
        {
            ImGui::PushID(i);

            float defaultValue = m_DisplayDefaults[i].load();
            float value = defaultValue + m_Offsets[i].load();
            if (ImGui::SliderFloat(k_Settings[i].Label, &value, defaultValue + k_Settings[i].Min, defaultValue + k_Settings[i].Max, "%.2f"))
            {
                m_Offsets[i] = value - defaultValue;
            }
            changed |= ImGui::IsItemDeactivatedAfterEdit();

            ImGui::SameLine();
            ImGui::BeginDisabled(m_Offsets[i].load() == 0.0f);
            if (ImGui::SmallButton("Reset"))
            {
                m_Offsets[i] = 0.0f;
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Back to this car's default (%.2f).", defaultValue);

            ImGui::PopID();
        }

        if (ImGui::Button("Reset all"))
        {
            for (std::atomic<float>& offset : m_Offsets)
            {
                offset = 0.0f;
            }
            changed = true;
        }

        if (changed)
        {
            SaveConfig();
        }
    }
    ImGui::End();
}
