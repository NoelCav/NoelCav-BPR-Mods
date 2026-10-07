#include "Borderless.hpp"

#include <exception>

#include <Windows.h>

#include "vendor/imgui.hpp"
#include "vendor/yaml-cpp.hpp"

#include "core/File.hpp"
#include "core/Pointer.hpp"
#include "mod-manager/ModManager.hpp"


namespace
{
    // The game's main window handle.
    constexpr uintptr_t k_GameWindow = 0x0139815C;

    // Rewrites a pending move/resize of a borderless (no caption, no sizing frame) window so it
    // covers its whole monitor. Windowed mode and minimizing are left alone.
    void FitBorderlessWindowToMonitor(HWND window, WINDOWPOS* windowPos)
    {
        LONG style = GetWindowLongA(window, GWL_STYLE);
        if ((style & (WS_CAPTION | WS_THICKFRAME)) != 0 || IsIconic(window) || windowPos->x <= -32000)
        {
            return;
        }

        MONITORINFO monitorInfo = { sizeof(monitorInfo) };
        if (!GetMonitorInfoA(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo))
        {
            return;
        }

        const RECT& monitor = monitorInfo.rcMonitor;
        windowPos->x = monitor.left;
        windowPos->y = monitor.top;
        windowPos->cx = monitor.right - monitor.left;
        windowPos->cy = monitor.bottom - monitor.top;
        windowPos->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
    }

    // Makes Windows send WM_WINDOWPOSCHANGING without the game asking, so the fit applies to a
    // window the game positioned before this mod loaded.
    void PokeWindow(HWND window)
    {
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}


Borderless Borderless::s_Instance;


Borderless::Borderless()
    :
    m_Logger(k_Name),
    m_ConfigDirectoryPath(ModManager::Get().GetConfigDirectoryPath().Append(k_ConfigDirectoryPath)),
    m_ConfigFilePath(Core::Path(m_ConfigDirectoryPath).Append("borderless-config.yaml"))
{
}

Borderless& Borderless::Get()
{
    return s_Instance;
}

void Borderless::Load()
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

        // The window doesn't exist yet when mods load; attach to it on the first game frame.
        ModManager::Get().GetHookManager().AddGameMainHook([]() { s_Instance.OnGameMain(); });
        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Borderless::Unload()
{
    // Only detach if nothing subclassed the window on top of us since.
    HWND window = static_cast<HWND>(m_Window);
    if (window != nullptr && GetWindowLongPtrA(window, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&WindowProc))
    {
        SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_OriginalWindowProc));
    }
    SaveConfig();
}

void Borderless::OnGameMain()
{
    if (m_Window == nullptr)
    {
        HWND window = Core::Pointer(k_GameWindow).as<HWND>();
        if (window == nullptr || !IsWindow(window))
        {
            return;
        }

        m_OriginalWindowProc = reinterpret_cast<void*>(SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
        if (m_OriginalWindowProc == nullptr)
        {
            m_Logger.Error("Failed to attach to the game window.");
            m_Window = window; // don't retry every frame
            return;
        }
        m_Window = window;
        m_RefitPending = true;
        m_Logger.Info("Attached to the game window.");
    }

    if (m_RefitPending.load() && m_OriginalWindowProc != nullptr)
    {
        m_RefitPending = false;
        PokeWindow(static_cast<HWND>(m_Window));
    }
}

long __stdcall Borderless::WindowProc(void* window, unsigned int message, unsigned int wParam, long lParam)
{
    HWND hwnd = static_cast<HWND>(window);
    if (message == WM_WINDOWPOSCHANGING && s_Instance.m_Enabled.load())
    {
        FitBorderlessWindowToMonitor(hwnd, reinterpret_cast<WINDOWPOS*>(static_cast<LPARAM>(lParam)));
    }
    return static_cast<long>(CallWindowProcA(reinterpret_cast<WNDPROC>(s_Instance.m_OriginalWindowProc), hwnd, message, wParam, lParam));
}

void Borderless::LoadConfig()
{
    try
    {
        YAML::Node yaml = YAML::Load(
            Core::File(m_ConfigFilePath, Core::File::Mode::Read, m_Logger).ReadAsText()
        );

        m_Enabled = yaml["Enabled"].as<bool>(m_Enabled.load());

        m_Logger.Info("Loaded borderless config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load borderless config, using defaults. exception: %s", ex.what());
    }
}

void Borderless::SaveConfig() const
{
    try
    {
        YAML::Node yaml;
        yaml["Enabled"] = m_Enabled.load();

        Core::File(m_ConfigFilePath, Core::File::Mode::Write, m_Logger).WriteAsText(
            YAML::Dump(yaml)
        );

        m_Logger.Info("Saved borderless config.");
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to save borderless config. exception: %s", ex.what());
    }
}

void Borderless::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(40.0f, 540.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 170.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);

        bool enabled = m_Enabled.load();
        if (ImGui::Checkbox("Fill the whole monitor in borderless mode", &enabled))
        {
            m_Enabled = enabled;
            m_RefitPending = enabled;
            SaveConfig();
        }
        ImGui::TextDisabled("Turning it off takes effect after a restart.");
    }
    ImGui::End();
}
