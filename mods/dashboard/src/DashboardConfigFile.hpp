#pragma once


#include <cstdint>

#include "core/Path.hpp"
#include "core/Logger.hpp"


// Layout units are "reference pixels": pixels on a 1080-pixel-tall screen, scaled with the
// actual screen height so the dials sit in the same place at any resolution.
enum class DashboardAnchor : int32_t
{
    BottomLeft = 0,
    BottomCenter = 1,
    BottomRight = 2,
};

struct DashboardGaugeConfig
{
    bool Enabled = true;
    float OffsetX = 0.0f; // dial center, from the anchor corner (towards the screen center is positive)
    float OffsetY = 0.0f; // dial center, up from the bottom of the screen
};

struct DashboardConfig
{
    bool AlwaysVisible = false;
    bool HideInShowtime = true;
    bool MetricUnits = false;
    float Opacity = 100.0f;
    float BackgroundOpacity = 55.0f;
    float Scale = 1.0f;
    DashboardAnchor Anchor = DashboardAnchor::BottomLeft;
    // Side by side on the left, above the boost bar and clear of the news ticker along the bottom.
    DashboardGaugeConfig Speedometer = { true, 130.0f, 300.0f };
    DashboardGaugeConfig Tachometer = { true, 345.0f, 300.0f };
    struct
    {
        uint32_t Dial = 0xFFFFFFFF;
        uint32_t Text = 0xFFFFFFFF;
        uint32_t Needle = 0xFF3426FF;
    } Colors;
};


class DashboardConfigFile
{
public:
    DashboardConfigFile(Core::Path directory, const Core::Logger& logger);

public:
    void Load();
    void Save() const;

    DashboardConfig& GetDashboardConfig();

private:
    Core::Path m_FilePath;
    const Core::Logger& m_Logger;

    DashboardConfig m_DashboardConfig;
};
