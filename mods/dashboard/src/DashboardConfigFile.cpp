#include "DashboardConfigFile.hpp"

#include <exception>

#include "vendor/yaml-cpp.hpp"

#include "core/File.hpp"


static constexpr char k_Name[] = "dashboard config";


DashboardConfigFile::DashboardConfigFile(Core::Path directory, const Core::Logger& logger)
    :
    m_FilePath(directory.Append("dashboard-config.yaml")),
    m_Logger(logger)
{
}

void DashboardConfigFile::Load()
{
    try
    {
        YAML::Node yaml = YAML::Load(
            Core::File(m_FilePath, Core::File::Mode::Read, m_Logger).ReadAsText()
        );

        const DashboardConfig defaults;

        m_DashboardConfig.AlwaysVisible = yaml["AlwaysVisible"].as<bool>(defaults.AlwaysVisible);
        m_DashboardConfig.HideInShowtime = yaml["HideInShowtime"].as<bool>(defaults.HideInShowtime);
        m_DashboardConfig.MetricUnits   = yaml["MetricUnits"].as<bool>(defaults.MetricUnits);
        m_DashboardConfig.Opacity       = yaml["Opacity"].as<float>(defaults.Opacity);
        m_DashboardConfig.BackgroundOpacity = yaml["BackgroundOpacity"].as<float>(defaults.BackgroundOpacity);
        m_DashboardConfig.Scale         = yaml["GaugeScale"].as<float>(defaults.Scale);
        m_DashboardConfig.Anchor        = static_cast<DashboardAnchor>(yaml["Anchor"].as<int32_t>(static_cast<int32_t>(defaults.Anchor)));

        auto loadGauge = [](const YAML::Node& node, const DashboardGaugeConfig& gaugeDefaults)
        {
            DashboardGaugeConfig gauge;
            gauge.Enabled = node["Enabled"].as<bool>(gaugeDefaults.Enabled);
            // OffsetX/OffsetY replaced the old bottom-center X/Y; old values are ignored.
            gauge.OffsetX = node["OffsetX"].as<float>(gaugeDefaults.OffsetX);
            gauge.OffsetY = node["OffsetY"].as<float>(gaugeDefaults.OffsetY);
            return gauge;
        };
        m_DashboardConfig.Speedometer = loadGauge(yaml["Speedometer"], defaults.Speedometer);
        m_DashboardConfig.Tachometer  = loadGauge(yaml["Tachometer"], defaults.Tachometer);

        YAML::Node colorsNode = yaml["Colors"];
        m_DashboardConfig.Colors.Dial   = colorsNode["Dial"].as<uint32_t>(defaults.Colors.Dial);
        m_DashboardConfig.Colors.Text   = colorsNode["Text"].as<uint32_t>(defaults.Colors.Text);
        m_DashboardConfig.Colors.Needle = colorsNode["Needle"].as<uint32_t>(defaults.Colors.Needle);

        m_Logger.Info("Loaded %s.", k_Name);
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load %s. exception: %s", k_Name, ex.what());
    }
}

void DashboardConfigFile::Save() const
{
    try
    {
        YAML::Node yaml;

        yaml["AlwaysVisible"] = m_DashboardConfig.AlwaysVisible;
        yaml["HideInShowtime"] = m_DashboardConfig.HideInShowtime;
        yaml["MetricUnits"]   = m_DashboardConfig.MetricUnits;
        yaml["Opacity"]       = m_DashboardConfig.Opacity;
        yaml["BackgroundOpacity"] = m_DashboardConfig.BackgroundOpacity;
        yaml["GaugeScale"]    = m_DashboardConfig.Scale;
        yaml["Anchor"]        = static_cast<int32_t>(m_DashboardConfig.Anchor);

        auto saveGauge = [](const DashboardGaugeConfig& gauge)
        {
            YAML::Node node;
            node["Enabled"] = gauge.Enabled;
            node["OffsetX"] = gauge.OffsetX;
            node["OffsetY"] = gauge.OffsetY;
            return node;
        };
        yaml["Speedometer"] = saveGauge(m_DashboardConfig.Speedometer);
        yaml["Tachometer"]  = saveGauge(m_DashboardConfig.Tachometer);

        YAML::Node colorsNode;
        colorsNode["Dial"]   = m_DashboardConfig.Colors.Dial;
        colorsNode["Text"]   = m_DashboardConfig.Colors.Text;
        colorsNode["Needle"] = m_DashboardConfig.Colors.Needle;
        yaml["Colors"] = colorsNode;

        Core::File(m_FilePath, Core::File::Mode::Write, m_Logger).WriteAsText(
            YAML::Dump(yaml)
        );

        m_Logger.Info("Saved %s.", k_Name);
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to save %s. exception: %s", k_Name, ex.what());
    }
}

DashboardConfig& DashboardConfigFile::GetDashboardConfig()
{
    return m_DashboardConfig;
}
