#include "Dashboard.hpp"

#include <cstring>
#include <exception>

#include "vendor/imgui.hpp"

#include "core/Pointer.hpp"
#include "mod-manager/ModManager.hpp"


Dashboard Dashboard::s_Instance;


Dashboard::Dashboard()
    :
    m_ConfigDirectoryPath(ModManager::Get().GetConfigDirectoryPath().Append(k_ConfigDirectoryPath)),
    m_Logger(k_Name),
    m_DashboardConfigFile(m_ConfigDirectoryPath, m_Logger),
    m_DashboardHud(m_DashboardConfigFile, m_Logger)
{
}

Dashboard& Dashboard::Get()
{
    return s_Instance;
}

void Dashboard::OnProcessAttach()
{
    PTHREAD_START_ROUTINE loadThreadProc = [](LPVOID lpThreadParameter) -> DWORD
    {
        s_Instance.Load();
        return 0;
    };
    m_LoadThreadHandle = CreateThread(nullptr, 0, loadThreadProc, nullptr, 0, nullptr);
}

void Dashboard::OnProcessDetach()
{
    Unload();
    CloseHandle(m_LoadThreadHandle);
}

void Dashboard::Load()
{
    try
    {
        m_Logger.Info("Loading...");

        if (!ModManager::Get().CheckVersion(k_Version))
        {
            throw std::exception("Mod Manager and Mod versions mismatch.");
        }

        if (!m_ConfigDirectoryPath.Exists())
        {
            m_ConfigDirectoryPath.CreateDirectoryTree();
            m_Logger.Info("Created config directory. path: '%s'", m_ConfigDirectoryPath.GetPath());
        }

        m_DashboardConfigFile.Load();

        // Wait to be in game.
        {
            m_Logger.Info("Waiting to be in game...");

            while (true)
            {
                // BrnGameMainFlowController::GameMainFlowController::meCurrentState
                Core::Pointer gameModule = 0x013FC8E0;
                if (
                    gameModule.as<void*>() != nullptr &&
                    gameModule.deref().at(0xB6D4C8).as<int32_t>() == 6 // BrnGameMainFlowController::EMainGameFlowState::E_MGS_IN_GAME
                )
                {
                    break;
                }

                Sleep(1000);
            }

            m_Logger.Info("In game.");
        }

        // Load the dial font (a Windows system font; the dials themselves are drawn in code).
        {
            EnterCriticalSection(ModManager::Get().GetImGuiManager().GetCriticalSection());

            m_DashboardHud.LoadFont();

            LeaveCriticalSection(ModManager::Get().GetImGuiManager().GetCriticalSection());
        }

        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
        ModManager::Get().GetImGuiManager().AddOverlay([]() { s_Instance.RenderOverlay(); });

        m_Logger.Info("Loaded.");
    }
    catch (const std::exception& e)
    {
        m_Logger.Error("%s", e.what());
        MessageBoxA(NULL, e.what(), k_Name, MB_ICONERROR);
    }
}

void Dashboard::Unload()
{
    try
    {
        m_DashboardConfigFile.Save();
    }
    catch (const std::exception& e)
    {
        m_Logger.Error("%s", e.what());
        MessageBoxA(NULL, e.what(), k_Name, MB_ICONERROR);
    }
}

void Dashboard::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(1320.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() / 2.0f);

        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);
        ImGui::Text("Framerate   %.1f", io.Framerate);

        m_DashboardHud.OnRenderMenu();

        ImGui::PopItemWidth();
    }
    ImGui::End();
}

void Dashboard::RenderOverlay()
{
    m_DashboardHud.OnRenderOverlay();
}
