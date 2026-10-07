#include "GameData.hpp"

#include <cstdio>
#include <cstring>
#include <map>

#include "bpr/CgsID.hpp"
#include "bpr/Language.hpp"

#include "core/Pointer.hpp"


namespace
{
    // All offsets are from BrnGame::BrnGameModule* ([0x013FC8E0]), PC/Steam build.
    constexpr uintptr_t k_GameModule = 0x013FC8E0;

    // BrnResource::VehicleList. +0x0 -> VehicleListResource { uint32 count; VehicleListEntry* entries; }.
    constexpr ptrdiff_t k_VehicleList = 0x68C350;
    constexpr ptrdiff_t k_VehicleListEntrySize = 0x108;

    // BrnProgression::Profile (found 2026-09-22 by scanning for the CarData array; a second,
    // older copy lives at +0x8681D0 and lags behind, so don't use that one).
    constexpr ptrdiff_t k_ProfileCarCount = 0x6A7E20;    // int32
    constexpr ptrdiff_t k_ProfileLiveryCount = 0x6A7E24; // int32
    constexpr ptrdiff_t k_ProfileCars = 0x6A7E38;        // BrnProgression::CarData[512], 0x18 each
    constexpr ptrdiff_t k_ProfileLiveries = 0x6AAE38;    // BrnProgression::LiveryData[512], 0x18 each
    constexpr int32_t k_ProfileCapacity = 512;
    constexpr ptrdiff_t k_ProfileEntrySize = 0x18;

    // Current player car.
    constexpr ptrdiff_t k_PlayerVehicleIndex = 0x40C28;
    constexpr ptrdiff_t k_ActiveRaceCars = 0x12980;
    constexpr ptrdiff_t k_ActiveRaceCarSize = 0x4180;
    constexpr ptrdiff_t k_ActiveRaceCarRaceCar = 0x7C0;
    constexpr ptrdiff_t k_RaceCarVehicleID = 0x68;
    constexpr ptrdiff_t k_RaceCarColourIndex = 0x94;
    constexpr ptrdiff_t k_RaceCarPaletteIndex = 0x98;

    // Game state.
    constexpr ptrdiff_t k_MainGameFlowState = 0xB6D4C8; // 6 = in game
    constexpr ptrdiff_t k_GameModeType = 0x69D58C;      // -1 = free roam
    constexpr ptrdiff_t k_IsOnline = 0xB6D415;
    constexpr ptrdiff_t k_ChallengeTimerRunning = 0x6A4104;
    constexpr ptrdiff_t k_CurrentFreeburnGame = 0x6EBE50;

    Core::Pointer GetGameModule()
    {
        return Core::Pointer(k_GameModule).deref();
    }

    Core::Pointer GetPlayerRaceCar()
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return nullptr;
        }

        uint32_t playerIndex = gameModule.at(k_PlayerVehicleIndex).as<uint32_t>();
        if (playerIndex >= 8)
        {
            return nullptr;
        }

        return gameModule.at(k_ActiveRaceCars + playerIndex * k_ActiveRaceCarSize).at(k_ActiveRaceCarRaceCar).deref();
    }

    // BrnWorld::GlobalColourPalette*: 5 x { Vector4* paint; Vector4* pearl; int32 count; }.
    // The car colour update (0x06A6DF..) reads it as [object + 0x3018C] with the object at
    // gm+0x10D00; found 2026-09-22 and checked against the live car's resolved paint.
    constexpr ptrdiff_t k_ColourPalettes = 0x40E8C;

    Core::Pointer GetPalettes()
    {
        Core::Pointer gameModule = GetGameModule();
        return gameModule.GetPointer() != nullptr ? gameModule.at(k_ColourPalettes).deref() : nullptr;
    }

    Core::Pointer FindProfileCar(Core::Pointer gameModule, uint64_t vehicleID)
    {
        int32_t count = gameModule.at(k_ProfileCarCount).as<int32_t>();
        if (count < 0 || count > k_ProfileCapacity)
        {
            return nullptr;
        }

        for (int32_t i = 0; i < count; ++i)
        {
            Core::Pointer car = gameModule.at(k_ProfileCars + i * k_ProfileEntrySize);
            if (car.as<uint64_t>() == vehicleID)
            {
                return car;
            }
        }

        return nullptr;
    }

    Core::Pointer FindProfileLivery(Core::Pointer gameModule, uint64_t carID)
    {
        int32_t count = gameModule.at(k_ProfileLiveryCount).as<int32_t>();
        if (count < 0 || count > k_ProfileCapacity)
        {
            return nullptr;
        }

        for (int32_t i = 0; i < count; ++i)
        {
            Core::Pointer livery = gameModule.at(k_ProfileLiveries + i * k_ProfileEntrySize);
            if (livery.as<uint64_t>() == carID)
            {
                return livery;
            }
        }

        return nullptr;
    }

    // int __cdecl GetVehicleManufacturer(CgsID) - index into the manufacturer icon list.
    // Address and name table from matty-ross's mod-menu (bpr-mods-repository).
    int32_t GetVehicleManufacturer(uint64_t vehicleID)
    {
        int32_t manufacturer = 9;

        __asm
        {
            push dword ptr [vehicleID + 0x4]
            push dword ptr [vehicleID + 0x0]

            mov eax, 0x00A69A20
            call eax
            add esp, 0x8

            mov dword ptr [manufacturer], eax
        }

        return manufacturer;
    }

    const char* GetManufacturerName(int32_t manufacturer)
    {
        static constexpr const char* names[] =
        {
            "Carson", "Hunter", "Jansen", "Krieger", "Kitano", "Montgomery", "Nakamura", "Rossolini", "Watson",
        };
        return (manufacturer >= 0 && manufacturer < 9) ? names[manufacturer] : nullptr;
    }

    std::string TitleCase(const char* text)
    {
        // Localized CAR_CAPS_* strings are upper case ("CAVALRY"); the Junkyard shows them as-is,
        // but mixed with a manufacturer they read better in title case. Keep digits/codes intact.
        std::string result = text;
        bool wordStart = true;
        for (char& c : result)
        {
            if (c >= 'A' && c <= 'Z' && !wordStart)
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
            wordStart = (c == ' ' || c == '-');
        }
        return result;
    }

    // Display name, after matty-ross's CreateVehicleName: the localized CAR_CAPS_<id> string
    // (from the parent for plain finishes) with the manufacturer in front, falling back to the
    // vehicle list's internal name.
    std::string CreateCarName(Core::Pointer entry)
    {
        uint64_t vehicleID = entry.at(0x0).as<uint64_t>();
        uint64_t parentID = entry.at(0x8).as<uint64_t>();

        char stringID[32] = "CAR_CAPS_";
        BPR::CgsID_Uncompress(vehicleID, stringID + 9);
        const char* localized = BPR::LanguageManager_FindString(stringID);

        if (localized == nullptr || localized[0] == '\0')
        {
            return entry.at(0x30).as<char[64]>();
        }

        std::string name = TitleCase(localized);
        const char* manufacturer = GetManufacturerName(GetVehicleManufacturer(parentID != 0 ? parentID : vehicleID));
        if (manufacturer != nullptr && name.rfind(manufacturer, 0) != 0)
        {
            name = std::string(manufacturer) + " " + name;
        }
        return name;
    }
}


namespace GameData
{
    bool VehicleCatalog::Build()
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return false;
        }

        Core::Pointer vehicleList = gameModule.at(k_VehicleList).deref();
        if (vehicleList.GetPointer() == nullptr)
        {
            return false;
        }

        uint32_t count = vehicleList.at(0x0).as<uint32_t>();
        Core::Pointer entries = vehicleList.at(0x4).deref();
        if (count == 0 || count > 4096 || entries.GetPointer() == nullptr)
        {
            return false;
        }

        m_Cars.clear();
        m_CarIndexByVehicle.clear();

        // Pass 1: every primary finish is a car, in vehicle list order (which is the game's
        // unlock order). Burning Route rewards and cop variants are cars of their own.
        for (uint32_t i = 0; i < count; ++i)
        {
            Core::Pointer entry = entries.at(i * k_VehicleListEntrySize);
            uint8_t livery = entry.at(0xFD).as<uint8_t>();
            uint32_t categories = entry.at(0xF8).as<uint32_t>();
            if ((livery != Livery_Primary && livery != Livery_PrimaryBurningRoute) || categories == 0)
            {
                continue; // secondary finishes are attached in pass 2; category 0 = unused/test cars
            }

            uint8_t carType = entry.at(0xFC).as<uint8_t>();

            Car car;
            car.ID = entry.at(0x0).as<uint64_t>();
            car.Name = CreateCarName(entry);
            car.Categories = categories;
            car.BoostType = carType & 0x0F;
            car.IsBike = (carType >> 4) == 1;
            car.Paintable = (entry.at(0x94).as<uint32_t>() & 0x20) != 0; // GamePlayData flags: paintable
            car.SpeedStat = entry.at(0x100).as<uint8_t>();
            car.BoostStat = entry.at(0x101).as<uint8_t>();
            car.StrengthStat = entry.at(0x9B).as<uint8_t>();
            car.Finishes.push_back(Finish{ car.ID, "Finish 1", entry.at(0x102).as<uint8_t>(), entry.at(0x103).as<uint8_t>() });

            m_CarIndexByVehicle[car.ID] = m_Cars.size();
            m_Cars.push_back(std::move(car));
        }

        // Pass 2: attach finishes to their parent car, and Burning Route rewards to their base car.
        std::map<size_t, int> secondaryCounts;
        std::map<size_t, int> communityCounts;
        for (uint32_t i = 0; i < count; ++i)
        {
            Core::Pointer entry = entries.at(i * k_VehicleListEntrySize);
            uint64_t vehicleID = entry.at(0x0).as<uint64_t>();
            uint64_t parentID = entry.at(0x8).as<uint64_t>();
            uint8_t livery = entry.at(0xFD).as<uint8_t>();

            auto parent = m_CarIndexByVehicle.find(parentID);
            if (parentID == 0 || parent == m_CarIndexByVehicle.end())
            {
                continue;
            }
            Car& parentCar = m_Cars[parent->second];

            if (livery == Livery_PrimaryBurningRoute)
            {
                // Cop variants share this livery type; the Burning Route reward is the Paradise one.
                auto reward = m_CarIndexByVehicle.find(vehicleID);
                if ((entry.at(0xF8).as<uint32_t>() & Category_ParadiseCars) != 0 && parentCar.BurningRouteCarID == 0 && reward != m_CarIndexByVehicle.end())
                {
                    parentCar.BurningRouteCarID = vehicleID;
                    m_Cars[reward->second].RewardForCarID = parentID;
                }
                continue;
            }

            char name[32] = {};
            switch (livery)
            {
            case Livery_Secondary: sprintf_s(name, "Finish %d", ++secondaryCounts[parent->second] + 1); break;
            case Livery_Platinum:  sprintf_s(name, "Platinum"); break;
            case Livery_Gold:      sprintf_s(name, "Gold"); break;
            case Livery_Community: sprintf_s(name, "Community %d", ++communityCounts[parent->second]); break;
            default: continue;
            }

            parentCar.Finishes.push_back(Finish{ vehicleID, name, entry.at(0x102).as<uint8_t>(), entry.at(0x103).as<uint8_t>() });
            m_CarIndexByVehicle[vehicleID] = parent->second;
        }

        return true;
    }

    const Car* VehicleCatalog::FindCarByVehicle(uint64_t vehicleID) const
    {
        auto it = m_CarIndexByVehicle.find(vehicleID);
        return it != m_CarIndexByVehicle.end() ? &m_Cars[it->second] : nullptr;
    }

    bool GetOwnedVehicles(std::unordered_map<uint64_t, OwnedVehicle>& owned)
    {
        owned.clear();

        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return false;
        }

        int32_t count = gameModule.at(k_ProfileCarCount).as<int32_t>();
        if (count < 0 || count > k_ProfileCapacity)
        {
            return false;
        }

        for (int32_t i = 0; i < count; ++i)
        {
            Core::Pointer car = gameModule.at(k_ProfileCars + i * k_ProfileEntrySize);
            owned[car.at(0x0).as<uint64_t>()] = OwnedVehicle
            {
                .ColourIndex = car.at(0x8).as<uint8_t>(),
                .PaletteIndex = car.at(0x9).as<uint8_t>(),
                .Damage = car.at(0xC).as<float>(),
            };
        }

        return true;
    }

    uint64_t GetChosenFinish(uint64_t carID)
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return 0;
        }

        Core::Pointer livery = FindProfileLivery(gameModule, carID);
        return livery.GetPointer() != nullptr ? livery.at(0x8).as<uint64_t>() : 0;
    }

    void SetChosenFinish(uint64_t carID, uint64_t finishID)
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return;
        }

        Core::Pointer livery = FindProfileLivery(gameModule, carID);
        if (livery.GetPointer() != nullptr)
        {
            livery.at(0x8).as<uint64_t>() = finishID;
        }
    }

    void SetVehicleColour(uint64_t vehicleID, uint8_t colourIndex, uint8_t paletteIndex)
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr)
        {
            return;
        }

        Core::Pointer car = FindProfileCar(gameModule, vehicleID);
        if (car.GetPointer() != nullptr)
        {
            car.at(0x8).as<uint8_t>() = colourIndex;
            car.at(0x9).as<uint8_t>() = paletteIndex;
        }
    }

    bool GetCurrentVehicle(uint64_t& vehicleID, int& colourIndex, int& paletteIndex)
    {
        Core::Pointer raceCar = GetPlayerRaceCar();
        if (raceCar.GetPointer() == nullptr)
        {
            return false;
        }

        vehicleID = raceCar.at(k_RaceCarVehicleID).as<uint64_t>();
        colourIndex = raceCar.at(k_RaceCarColourIndex).as<int32_t>();
        paletteIndex = raceCar.at(k_RaceCarPaletteIndex).as<int32_t>();
        return true;
    }

    void SetCurrentVehicleColour(int colourIndex, int paletteIndex)
    {
        Core::Pointer raceCar = GetPlayerRaceCar();
        if (raceCar.GetPointer() != nullptr)
        {
            raceCar.at(k_RaceCarColourIndex).as<int32_t>() = colourIndex;
            raceCar.at(k_RaceCarPaletteIndex).as<int32_t>() = paletteIndex;
        }
    }

    int GetPaletteColourCount(int palette)
    {
        Core::Pointer palettes = GetPalettes();
        if (palettes.GetPointer() == nullptr || palette < 0 || palette >= k_PaletteCount)
        {
            return 0;
        }

        int32_t count = palettes.at(palette * 0xC + 0x8).as<int32_t>();
        return (count > 0 && count <= 255) ? count : 0;
    }

    bool GetPaletteColour(int palette, int index, float paint[3], float pearl[3])
    {
        if (index < 0 || index >= GetPaletteColourCount(palette))
        {
            return false;
        }

        Core::Pointer entry = GetPalettes().at(palette * 0xC);
        std::memcpy(paint, entry.at(0x0).deref().at(index * 0x10).GetPointer(), sizeof(float) * 3);
        std::memcpy(pearl, entry.at(0x4).deref().at(index * 0x10).GetPointer(), sizeof(float) * 3);
        return true;
    }

    const char* GetSwitchBlockedReason()
    {
        Core::Pointer gameModule = GetGameModule();
        if (gameModule.GetPointer() == nullptr || gameModule.at(k_MainGameFlowState).as<int32_t>() != 6)
        {
            return "Not in game.";
        }
        if (gameModule.at(k_IsOnline).as<bool>())
        {
            return "Not available online.";
        }
        if (gameModule.at(k_GameModeType).as<int32_t>() != -1)
        {
            return "Only available in free roam (not in Showtime or an event).";
        }
        if (gameModule.at(k_ChallengeTimerRunning).as<bool>() || gameModule.at(k_CurrentFreeburnGame).as<void*>() != nullptr)
        {
            return "Not available during a challenge.";
        }
        if (GetPlayerRaceCar().GetPointer() == nullptr)
        {
            return "No player car.";
        }
        Core::Pointer raceMainHudState = gameModule.at(0x7FABBC).as<void*>(); // BrnGui::RaceMainHudState*
        if (raceMainHudState.GetPointer() == nullptr || !raceMainHudState.at(0x14C).as<bool>())
        {
            return "Not available in a Junkyard or menu.";
        }
        return nullptr;
    }

    std::string VehicleIDToString(uint64_t vehicleID)
    {
        char text[16] = {};
        BPR::CgsID_Uncompress(vehicleID, text);
        return text;
    }
}
