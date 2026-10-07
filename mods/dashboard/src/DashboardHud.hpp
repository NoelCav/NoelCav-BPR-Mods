#pragma once


#include "vendor/imgui.hpp"

#include "core/Logger.hpp"

#include "DashboardConfigFile.hpp"


// Speedometer and tachometer drawn entirely with ImGui vector primitives (no textures), so
// they stay sharp at any size.
class DashboardHud
{
public:
    DashboardHud(DashboardConfigFile& dashboardConfigFile, const Core::Logger& logger);

public:
    void LoadFont();

    void OnRenderMenu();
    void OnRenderOverlay();

private:
    struct DialSpec
    {
        float MaxValue;
        float MinorStep;
        float MajorStep;
        float LabelDivisor; // label = value / divisor
        float RedlineFrom;  // <= 0 for none
    };

    ImVec2 GetGaugeCenter(const DashboardGaugeConfig& gauge) const;
    float GetUnit() const;
    float GetRadius() const;

    void HandleGaugeDragging();

    void RenderDial(const ImVec2& center, const DialSpec& spec, float value) const;
    void RenderText(const ImVec2& center, float offsetY, const char* text, float size, ImU32 color) const;

private:
    DashboardConfigFile& m_DashboardConfigFile;
    const Core::Logger& m_Logger;

    ImFont* m_Font = nullptr;

    int m_MenuFrame = -10;              // last frame the Dashboard menu was drawn
    DashboardGaugeConfig* m_DraggedGauge = nullptr;
};
