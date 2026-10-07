#include "DashboardHud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <Windows.h>

#include "core/Pointer.hpp"


namespace
{
    constexpr float k_Pi = 3.14159265f;

    // The dial sweeps 270 degrees clockwise from bottom-left to bottom-right, leaving the gap
    // at the bottom for the unit label. Angles are in screen space (y down).
    constexpr float k_StartAngle = 135.0f * k_Pi / 180.0f;
    constexpr float k_SweepAngle = 270.0f * k_Pi / 180.0f;

    constexpr float k_ReferenceHeight = 1080.0f;
    constexpr float k_BaseRadius = 100.0f; // reference pixels at scale 1

    ImU32 WithOpacity(ImU32 color, float opacity)
    {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        value.w *= std::clamp(opacity, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    ImVec2 OnCircle(const ImVec2& center, float radius, float angle)
    {
        return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    }

    // Windows' own fonts, so the mod ships no font file. Bahnschrift (DIN-style) suits gauges.
    std::string FindSystemFont()
    {
        char windowsDirectory[MAX_PATH] = {};
        GetWindowsDirectoryA(windowsDirectory, MAX_PATH);

        for (const char* name : { "bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf" })
        {
            std::string path = std::string(windowsDirectory) + "\\Fonts\\" + name;
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                return path;
            }
        }
        return {};
    }
}


DashboardHud::DashboardHud(DashboardConfigFile& dashboardConfigFile, const Core::Logger& logger)
    :
    m_DashboardConfigFile(dashboardConfigFile),
    m_Logger(logger)
{
}

void DashboardHud::LoadFont()
{
    std::string path = FindSystemFont();
    if (path.empty())
    {
        m_Logger.Warning("No system font found, using the ImGui default font.");
        return;
    }

    m_Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str());
    m_Logger.Info("Loaded font '%s'.", path.c_str());
}

float DashboardHud::GetUnit() const
{
    return ImGui::GetMainViewport()->Size.y / k_ReferenceHeight;
}

float DashboardHud::GetRadius() const
{
    return k_BaseRadius * m_DashboardConfigFile.GetDashboardConfig().Scale * GetUnit();
}

ImVec2 DashboardHud::GetGaugeCenter(const DashboardGaugeConfig& gauge) const
{
    const DashboardConfig& config = m_DashboardConfigFile.GetDashboardConfig();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float unit = GetUnit();

    float x = 0.0f;
    switch (config.Anchor)
    {
    case DashboardAnchor::BottomCenter: x = viewport->Pos.x + viewport->Size.x / 2.0f + gauge.OffsetX * unit; break;
    case DashboardAnchor::BottomRight:  x = viewport->Pos.x + viewport->Size.x - gauge.OffsetX * unit; break;
    default:                            x = viewport->Pos.x + gauge.OffsetX * unit; break;
    }
    float y = viewport->Pos.y + viewport->Size.y - gauge.OffsetY * unit;

    return ImVec2(x, y);
}

void DashboardHud::OnRenderMenu()
{
    m_MenuFrame = ImGui::GetFrameCount();

    DashboardConfig& config = m_DashboardConfigFile.GetDashboardConfig();

    if (ImGui::Button("Save##dashboard-config-file"))
    {
        m_DashboardConfigFile.Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load##dashboard-config-file"))
    {
        m_DashboardConfigFile.Load();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset layout"))
    {
        const DashboardConfig defaults;
        config.Anchor = defaults.Anchor;
        config.Scale = defaults.Scale;
        config.Speedometer = defaults.Speedometer;
        config.Tachometer = defaults.Tachometer;
        m_DashboardConfigFile.Save();
    }

    ImGui::TextWrapped("While this window is open you can drag the dials with the mouse.");

    ImGui::SeparatorText("Display");
    ImGui::Checkbox("Always Visible", &config.AlwaysVisible);
    ImGui::Checkbox("Hide In Showtime", &config.HideInShowtime);
    ImGui::Checkbox("Metric Units", &config.MetricUnits);
    ImGui::SliderFloat("Opacity", &config.Opacity, 0.0f, 100.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Background Opacity", &config.BackgroundOpacity, 0.0f, 100.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Size", &config.Scale, 0.4f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

    static constexpr const char* anchors[] = { "Bottom left", "Bottom center", "Bottom right" };
    int anchor = std::clamp(static_cast<int>(config.Anchor), 0, 2);
    if (ImGui::Combo("Anchor", &anchor, anchors, IM_ARRAYSIZE(anchors)))
    {
        config.Anchor = static_cast<DashboardAnchor>(anchor);
    }

    auto renderGaugeConfig = [](const char* label, DashboardGaugeConfig& gauge)
    {
        ImGui::PushID(label);
        ImGui::SeparatorText(label);
        ImGui::Checkbox("Enabled", &gauge.Enabled);
        ImGui::DragFloat("Offset X", &gauge.OffsetX, 1.0f, -2000.0f, 2000.0f, "%.0f");
        ImGui::DragFloat("Offset Y", &gauge.OffsetY, 1.0f, 0.0f, 1080.0f, "%.0f");
        ImGui::PopID();
    };
    renderGaugeConfig("Speedometer", config.Speedometer);
    renderGaugeConfig("Tachometer", config.Tachometer);

    auto renderColorEdit = [](const char* label, uint32_t& configColor)
    {
        ImVec4 color = ImGui::ColorConvertU32ToFloat4(configColor);
        if (ImGui::ColorEdit3(label, &color.x))
        {
            configColor = ImGui::ColorConvertFloat4ToU32(color);
        }
    };
    ImGui::SeparatorText("Colors");
    renderColorEdit("Dial", config.Colors.Dial);
    renderColorEdit("Text", config.Colors.Text);
    renderColorEdit("Needle", config.Colors.Needle);
}

void DashboardHud::HandleGaugeDragging()
{
    DashboardConfig& config = m_DashboardConfigFile.GetDashboardConfig();
    ImGuiIO& io = ImGui::GetIO();

    bool menuOpen = ImGui::GetFrameCount() - m_MenuFrame <= 1;
    if (!menuOpen)
    {
        m_DraggedGauge = nullptr;
        return;
    }

    if (m_DraggedGauge == nullptr && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
    {
        float radius = GetRadius();
        for (DashboardGaugeConfig* gauge : { &config.Speedometer, &config.Tachometer })
        {
            ImVec2 center = GetGaugeCenter(*gauge);
            float dx = io.MousePos.x - center.x;
            float dy = io.MousePos.y - center.y;
            if (gauge->Enabled && dx * dx + dy * dy <= radius * radius)
            {
                m_DraggedGauge = gauge;
                break;
            }
        }
    }

    if (m_DraggedGauge != nullptr)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float unit = GetUnit();
            float directionX = config.Anchor == DashboardAnchor::BottomRight ? -1.0f : 1.0f;
            m_DraggedGauge->OffsetX += directionX * io.MouseDelta.x / unit;
            m_DraggedGauge->OffsetY -= io.MouseDelta.y / unit;
        }
        else
        {
            m_DraggedGauge = nullptr;
            m_DashboardConfigFile.Save();
        }
    }
}

void DashboardHud::OnRenderOverlay()
{
    const DashboardConfig& config = m_DashboardConfigFile.GetDashboardConfig();

    Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref(); // BrnGame::BrnGameModule*
    if (gameModule.GetPointer() == nullptr)
    {
        return;
    }

    Core::Pointer guiPlayerInfo = gameModule.at(0x8EFEC0); // BrnGui::GuiPlayerInfo
    Core::Pointer raceMainHudState = gameModule.at(0x7FABBC).as<void*>(); // BrnGui::RaceMainHudState*

    HandleGaugeDragging();
    bool menuOpen = ImGui::GetFrameCount() - m_MenuFrame <= 1;

    if (config.HideInShowtime && !menuOpen)
    {
        // Current event/game mode type: -1 in free-roam, 2 in Showtime (found 2026-09-21).
        if (gameModule.at(0x69D58C).as<int32_t>() == 2)
        {
            return;
        }
    }

    if (!config.AlwaysVisible && !menuOpen)
    {
        bool inRaceHud = raceMainHudState.GetPointer() != nullptr && raceMainHudState.at(0x14C).as<bool>();
        int32_t engineState = guiPlayerInfo.at(0x48).as<int32_t>();
        if (!inRaceHud || engineState == 0) // BrnGui::GuiPlayerEngineEvent::EEngineState::E_ENGINE_OFF
        {
            return;
        }
    }

    float opacity = config.Opacity / 100.0f;
    float radius = GetRadius();

    if (config.Speedometer.Enabled)
    {
        int32_t speed = std::abs(guiPlayerInfo.at(0x30).as<int32_t>());
        if (config.MetricUnits)
        {
            speed = static_cast<int32_t>(std::round(speed * 1.609f));
        }

        static constexpr DialSpec mph = { 240.0f, 10.0f, 40.0f, 1.0f, 0.0f };
        static constexpr DialSpec kmh = { 360.0f, 20.0f, 60.0f, 1.0f, 0.0f };

        char speedText[8] = {};
        sprintf_s(speedText, "%d", speed);

        ImVec2 center = GetGaugeCenter(config.Speedometer);
        RenderDial(center, config.MetricUnits ? kmh : mph, static_cast<float>(speed));
        RenderText(center, 0.0f, speedText, radius * 0.46f, WithOpacity(config.Colors.Text, opacity));
        RenderText(center, radius * 0.66f, config.MetricUnits ? "km/h" : "mph", radius * 0.17f, WithOpacity(config.Colors.Text, opacity * 0.8f));
    }

    if (config.Tachometer.Enabled)
    {
        int32_t rpm = guiPlayerInfo.at(0x34).as<int32_t>();
        int32_t gear = guiPlayerInfo.at(0x38).as<int32_t>();

        static constexpr char gears[][2] = { "R", "1", "2", "3", "4", "5", "6" };
        static constexpr DialSpec tach = { 12000.0f, 500.0f, 1000.0f, 1000.0f, 10000.0f };

        ImVec2 center = GetGaugeCenter(config.Tachometer);
        RenderDial(center, tach, static_cast<float>(rpm));
        RenderText(center, 0.0f, gears[std::clamp(gear, 0, 6)], radius * 0.46f, WithOpacity(config.Colors.Text, opacity));
        RenderText(center, radius * 0.66f, "x1000 rpm", radius * 0.15f, WithOpacity(config.Colors.Text, opacity * 0.8f));
    }

    // Outline the dials while they can be dragged.
    if (menuOpen)
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        for (const DashboardGaugeConfig* gauge : { &config.Speedometer, &config.Tachometer })
        {
            if (gauge->Enabled)
            {
                bool dragged = gauge == m_DraggedGauge;
                drawList->AddCircle(GetGaugeCenter(*gauge), radius + 4.0f, dragged ? IM_COL32(255, 220, 80, 255) : IM_COL32(255, 255, 255, 90), 0, 2.0f);
            }
        }
    }
}

void DashboardHud::RenderDial(const ImVec2& center, const DialSpec& spec, float value) const
{
    const DashboardConfig& config = m_DashboardConfigFile.GetDashboardConfig();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    float opacity = config.Opacity / 100.0f;
    float radius = GetRadius();
    auto angleOf = [&](float v) { return k_StartAngle + k_SweepAngle * std::clamp(v / spec.MaxValue, 0.0f, 1.0f); };

    ImU32 dialColor = WithOpacity(config.Colors.Dial, opacity);
    ImU32 dimDialColor = WithOpacity(config.Colors.Dial, opacity * 0.55f);
    ImU32 needleColor = WithOpacity(config.Colors.Needle, opacity);
    ImU32 redlineColor = WithOpacity(IM_COL32(230, 45, 45, 255), opacity * 0.85f);

    // Face.
    drawList->AddCircleFilled(center, radius, WithOpacity(IM_COL32(8, 10, 14, 255), opacity * config.BackgroundOpacity / 100.0f), 64);

    // Outer track, redline band and the value arc.
    drawList->PathArcTo(center, radius * 0.965f, k_StartAngle, k_StartAngle + k_SweepAngle, 64);
    drawList->PathStroke(dimDialColor, 0, radius * 0.012f);

    if (spec.RedlineFrom > 0.0f)
    {
        drawList->PathArcTo(center, radius * 0.915f, angleOf(spec.RedlineFrom), angleOf(spec.MaxValue), 24);
        drawList->PathStroke(redlineColor, 0, radius * 0.07f);
    }

    if (value > 0.0f)
    {
        drawList->PathArcTo(center, radius * 0.965f, k_StartAngle, angleOf(value), 64);
        drawList->PathStroke(needleColor, 0, radius * 0.035f);
    }

    // Ticks and labels.
    float labelSize = radius * 0.15f;
    int minorCount = static_cast<int>(std::round(spec.MaxValue / spec.MinorStep));
    int majorEvery = static_cast<int>(std::round(spec.MajorStep / spec.MinorStep));
    for (int i = 0; i <= minorCount; ++i)
    {
        float tickValue = i * spec.MinorStep;
        float angle = angleOf(tickValue);
        bool major = i % majorEvery == 0;

        drawList->AddLine(
            OnCircle(center, radius * (major ? 0.80f : 0.86f), angle),
            OnCircle(center, radius * 0.93f, angle),
            major ? dialColor : dimDialColor,
            radius * (major ? 0.022f : 0.012f)
        );

        if (major)
        {
            char label[8] = {};
            sprintf_s(label, "%d", static_cast<int>(std::round(tickValue / spec.LabelDivisor)));

            ImFont* font = m_Font != nullptr ? m_Font : ImGui::GetFont();
            ImVec2 textSize = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label);
            ImVec2 position = OnCircle(center, radius * 0.66f, angle);
            drawList->AddText(font, labelSize, ImVec2(position.x - textSize.x / 2.0f, position.y - textSize.y / 2.0f), dialColor, label);
        }
    }

    // Needle: a tapered blade from the inner ring to the ticks.
    float angle = angleOf(value);
    ImVec2 direction = ImVec2(std::cos(angle), std::sin(angle));
    ImVec2 normal = ImVec2(-direction.y, direction.x);
    ImVec2 base = OnCircle(center, radius * 0.42f, angle);
    ImVec2 tip = OnCircle(center, radius * 0.95f, angle);
    float baseHalfWidth = radius * 0.03f;
    float tipHalfWidth = radius * 0.008f;
    drawList->AddQuadFilled(
        ImVec2(base.x + normal.x * baseHalfWidth, base.y + normal.y * baseHalfWidth),
        ImVec2(tip.x + normal.x * tipHalfWidth, tip.y + normal.y * tipHalfWidth),
        ImVec2(tip.x - normal.x * tipHalfWidth, tip.y - normal.y * tipHalfWidth),
        ImVec2(base.x - normal.x * baseHalfWidth, base.y - normal.y * baseHalfWidth),
        needleColor
    );
}

void DashboardHud::RenderText(const ImVec2& center, float offsetY, const char* text, float size, ImU32 color) const
{
    ImFont* font = m_Font != nullptr ? m_Font : ImGui::GetFont();
    ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);

    ImGui::GetForegroundDrawList()->AddText(
        font,
        size,
        ImVec2(center.x - textSize.x / 2.0f, center.y + offsetY - textSize.y / 2.0f),
        color,
        text
    );
}
