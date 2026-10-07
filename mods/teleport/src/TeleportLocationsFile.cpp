#include <exception>
#include <span>
#include <string>
#include <vector>

#include "vendor/yaml-cpp.hpp"

#include "core/Path.hpp"
#include "core/Logger.hpp"
#include "core/File.hpp"

#include "TeleportLocationsFile.hpp"


TeleportLocationsFile::TeleportLocationsFile(Core::Path configDirectoryPath, const Core::Logger& logger)
    :
    m_FilePath(configDirectoryPath.Append("teleport-locations.yaml")),
    m_Logger(logger)
{
}

std::span<TeleportLocationsFile::Location> TeleportLocationsFile::GetLocations()
{
    return m_Locations;
}

void TeleportLocationsFile::AddLocation(const Location& location)
{
    m_Locations.push_back(location);
}

void TeleportLocationsFile::RemoveLocationByName(const std::string& name)
{
    std::erase_if(m_Locations, [&](const Location& location)
    {
        return location.Name == name;
    });
}

void TeleportLocationsFile::Load()
{
    try
    {
        YAML::Node yaml = YAML::Load(
            Core::File(m_FilePath, Core::File::Mode::Read, m_Logger).ReadAsText()
        );

        m_Locations.clear();
        for (YAML::Node locationNode : yaml["Locations"])
        {
            Location location;
            location.Name = locationNode["Name"].as<std::string>("");
            for (int i = 0; i < 3; ++i)
            {
                location.Position[i] = locationNode["Position"][i].as<float>(0.0f);
                location.Direction[i] = locationNode["Direction"][i].as<float>(0.0f);
            }
            m_Locations.push_back(location);
        }

        m_Logger.Info("Loaded %s.", k_Name);
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load %s. exception: %s", k_Name, ex.what());
    }
}

void TeleportLocationsFile::Save() const
{
    try
    {
        YAML::Node yaml;

        for (const Location& location : m_Locations)
        {
            YAML::Node locationNode;
            locationNode["Name"] = location.Name;
            for (int i = 0; i < 3; ++i)
            {
                locationNode["Position"].push_back(location.Position[i]);
                locationNode["Direction"].push_back(location.Direction[i]);
            }
            yaml["Locations"].push_back(locationNode);
        }

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
