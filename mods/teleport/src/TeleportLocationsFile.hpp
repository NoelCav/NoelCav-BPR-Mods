#pragma once


#include <span>
#include <string>
#include <vector>

#include "core/Path.hpp"
#include "core/Logger.hpp"


class TeleportLocationsFile
{
public:
    struct Location
    {
        std::string Name;
        float Position[3] = {};
        float Direction[3] = {};
    };

public:
    TeleportLocationsFile(Core::Path configDirectoryPath, const Core::Logger& logger);

public:
    std::span<Location> GetLocations();
    void AddLocation(const Location& location);
    void RemoveLocationByName(const std::string& name);

    void Load();
    void Save() const;

private:
    static constexpr char k_Name[] = "teleport locations";

private:
    Core::Path m_FilePath;

    std::vector<Location> m_Locations;

    const Core::Logger& m_Logger;
};
