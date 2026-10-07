#include "Controls.hpp"

#include <cstdint>
#include <exception>

#include <Windows.h>
#include <Xinput.h>

#include "vendor/imgui.hpp"
#include "vendor/yaml-cpp.hpp"

#include "core/File.hpp"
#include "core/Pointer.hpp"
#include "mod-manager/ModManager.hpp"


namespace
{
    // BurnoutPR.exe keeps its pointer to XINPUT1_3!XInputGetState (ordinal 2) in this slot.
    // Every controller read goes through it, so swapping it lets us edit the stick values
    // before the game sees them. (Found 2026-09-21; ordinal 100 / XInputGetStateEx is unused.)
    constexpr uintptr_t k_XInputGetStateSlot = 0x00CAE69C;

    using XInputGetStateFn = DWORD(WINAPI*)(DWORD userIndex, XINPUT_STATE* state);
    XInputGetStateFn s_OriginalXInputGetState = nullptr;

    SHORT NegateAxis(SHORT value)
    {
        return value == -32768 ? 32767 : static_cast<SHORT>(-value);
    }

    bool WriteSlot(uintptr_t slot, void* value)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(slot), sizeof(void*), PAGE_READWRITE, &oldProtect))
        {
            return false;
        }
        *reinterpret_cast<void**>(slot) = value;
        VirtualProtect(reinterpret_cast<void*>(slot), sizeof(void*), oldProtect, &oldProtect);
        return true;
    }
}


Controls Controls::s_Instance;


Controls::Controls()
    :
    m_Logger(k_Name),
    m_ConfigDirectoryPath(ModManager::Get().GetConfigDirectoryPath().Append(k_ConfigDirectoryPath)),
    m_ConfigFilePath(Core::Path(m_ConfigDirectoryPath).Append("controls-config.yaml"))
{
}

Controls& Controls::Get()
{
    return s_Instance;
}

void Controls::Load()
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
        InstallXInputHook();

        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Controls::Unload()
{
    RemoveXInputHook();
    SaveConfig();
}

void Controls::InstallXInputHook()
{
    // Only hook if the slot really holds XInputGetState, or another mod's hook chained onto it
    // (Teleport hooks it too), so a different game build fails safe.
    HMODULE xinput = GetModuleHandleA("XINPUT1_3.dll");
    void* expected = xinput != nullptr ? reinterpret_cast<void*>(GetProcAddress(xinput, MAKEINTRESOURCEA(2))) : nullptr;
    void* current = *reinterpret_cast<void**>(k_XInputGetStateSlot);
    HMODULE currentModule = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(current), &currentModule);
    bool chainedHook = currentModule != nullptr && currentModule != GetModuleHandleA(nullptr);
    if (expected == nullptr || (current != expected && !chainedHook))
    {
        m_Logger.Error("XInputGetState slot mismatch, look inversion disabled. slot: 0x%p, expected: 0x%p", current, expected);
        return;
    }

    s_OriginalXInputGetState = reinterpret_cast<XInputGetStateFn>(current);
    if (!WriteSlot(k_XInputGetStateSlot, reinterpret_cast<void*>(&Hook_XInputGetState)))
    {
        m_Logger.Error("Failed to hook XInputGetState.");
        return;
    }

    m_HookInstalled = true;
    m_Logger.Info("Hooked XInputGetState. original: 0x%p", current);
}

void Controls::RemoveXInputHook()
{
    // Only unhook if nothing chained on top of us since; otherwise leave the chain intact.
    if (m_HookInstalled && *reinterpret_cast<void**>(k_XInputGetStateSlot) == reinterpret_cast<void*>(&Hook_XInputGetState))
    {
        WriteSlot(k_XInputGetStateSlot, reinterpret_cast<void*>(s_OriginalXInputGetState));
    }
    m_HookInstalled = false;
}

bool Controls::IsInShowtime()
{
    // BrnGameModule current game mode type: -1 free-roam, 2 Showtime, 8 Marked Man, ...
    Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref();
    return gameModule.GetPointer() != nullptr && gameModule.at(0x69D58C).as<int32_t>() == 2;
}

unsigned long __stdcall Controls::Hook_XInputGetState(unsigned long userIndex, void* state)
{
    DWORD result = s_OriginalXInputGetState(userIndex, static_cast<XINPUT_STATE*>(state));
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    XINPUT_GAMEPAD& gamepad = static_cast<XINPUT_STATE*>(state)->Gamepad;

    if (s_Instance.m_InvertLookVertical.load())
    {
        gamepad.sThumbRY = NegateAxis(gamepad.sThumbRY);
    }

    bool invertHorizontal = IsInShowtime() ? s_Instance.m_InvertLookHorizontalShowtime.load() : s_Instance.m_InvertLookHorizontalDriving.load();
    if (invertHorizontal)
    {
        gamepad.sThumbRX = NegateAxis(gamepad.sThumbRX);
    }

    return result;
}

void Controls::LoadConfig()
{
    try
    {
        YAML::Node yaml = YAML::Load(
            Core::File(m_ConfigFilePath, Core::File::Mode::Read, m_Logger).ReadAsText()
        );

        m_InvertLookVertical = yaml["InvertLookVertical"].as<bool>(m_InvertLookVertical.load());
        m_InvertLookHorizontalDriving = yaml["InvertLookHorizontalDriving"].as<bool>(m_InvertLookHorizontalDriving.load());
        m_InvertLookHorizontalShowtime = yaml["InvertLookHorizontalShowtime"].as<bool>(m_InvertLookHorizontalShowtime.load());

        m_Logger.Info("Loaded controls config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load controls config, using defaults. exception: %s", ex.what());
    }
}

void Controls::SaveConfig() const
{
    try
    {
        YAML::Node yaml;
        yaml["InvertLookVertical"] = m_InvertLookVertical.load();
        yaml["InvertLookHorizontalDriving"] = m_InvertLookHorizontalDriving.load();
        yaml["InvertLookHorizontalShowtime"] = m_InvertLookHorizontalShowtime.load();

        Core::File(m_ConfigFilePath, Core::File::Mode::Write, m_Logger).WriteAsText(
            YAML::Dump(yaml)
        );

        m_Logger.Info("Saved controls config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to save controls config. exception: %s", ex.what());
    }
}

void Controls::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(1320.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 280.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);

        ImGui::SeparatorText("Camera look (right stick)");

        if (!m_HookInstalled)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Controller hook not installed - see the log.");
        }

        bool changed = false;
        auto renderToggle = [&changed](const char* label, std::atomic<bool>& value)
        {
            bool v = value.load();
            if (ImGui::Checkbox(label, &v))
            {
                value = v;
                changed = true;
            }
        };

        renderToggle("Invert up/down (everywhere)", m_InvertLookVertical);
        renderToggle("Invert left/right while driving", m_InvertLookHorizontalDriving);
        renderToggle("Invert left/right in Showtime", m_InvertLookHorizontalShowtime);

        ImGui::Text("Showtime active: %s", IsInShowtime() ? "yes" : "no");

        if (changed)
        {
            SaveConfig();
        }
    }
    ImGui::End();
}
