#pragma once


#include <Windows.h>

#include "core/Path.hpp"
#include "core/Logger.hpp"

#include "DashboardConfigFile.hpp"
#include "DashboardHud.hpp"


class Dashboard
{
private:
    Dashboard();

public:
    static Dashboard& Get();

public:
    void OnProcessAttach();
    void OnProcessDetach();

private:
    void Load();
    void Unload();

    void RenderMenu();
    void RenderOverlay();

private:
    static constexpr char k_Name[] = "Dashboard";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "PISros0724 (Matty), NoelCav";
    static constexpr char k_ConfigDirectoryPath[] = "dashboard\\";

    static Dashboard s_Instance;

private:
    Core::Path m_ConfigDirectoryPath;
    Core::Logger m_Logger;

    DashboardConfigFile m_DashboardConfigFile;

    DashboardHud m_DashboardHud;

    HANDLE m_LoadThreadHandle = NULL;
};
