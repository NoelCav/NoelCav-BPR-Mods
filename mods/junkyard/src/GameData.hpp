#pragma once


#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


// Read-only views of the game's vehicle list and the player's progression profile,
// plus the handful of profile writes a real Junkyard makes (paint, chosen finish).
// Everything here is plain memory access; callers decide which thread it runs on.
namespace GameData
{
    // BrnResource::VehicleListEntry::meCategory (bit flags, a vehicle can be in several tabs).
    enum Category : uint32_t
    {
        Category_ParadiseCars  = 0x01,
        Category_ParadiseBikes = 0x02,
        Category_OnlineCars    = 0x04,
        Category_Toys          = 0x08,
        Category_Legendary     = 0x10,
        Category_BoostSpecials = 0x20,
        Category_Cops          = 0x40,
        Category_BigSurf       = 0x80,
    };

    // BrnResource::VehicleListEntry::muLiveryType.
    enum LiveryType : uint8_t
    {
        Livery_Primary             = 0,
        Livery_Secondary           = 1,
        Livery_PrimaryBurningRoute = 2, // Burning Route reward cars and cop variants
        Livery_Platinum            = 3,
        Livery_Gold                = 4,
        Livery_Community           = 5,
    };

    struct Finish
    {
        uint64_t ID = 0;
        std::string Name; // "Finish 1", "Platinum", ...
        uint8_t DefaultColourIndex = 0;
        uint8_t DefaultPaletteIndex = 0;
    };

    // One pickable car as the Junkyard shows it: a primary vehicle plus its finishes.
    struct Car
    {
        uint64_t ID = 0;           // the primary finish's vehicle ID
        std::string Name;
        uint32_t Categories = 0;
        int BoostType = 0;         // BPR::BoostType
        bool IsBike = false;
        bool Paintable = true;
        uint8_t SpeedStat = 0;     // Junkyard stat bars, 1-10
        uint8_t BoostStat = 0;
        uint8_t StrengthStat = 0;
        std::vector<Finish> Finishes;   // Finishes[0] is the car itself
        uint64_t BurningRouteCarID = 0; // this car's Burning Route reward, 0 if none
        uint64_t RewardForCarID = 0;    // if this car is a Burning Route reward: the car it upgrades
    };

    // BrnProgression::CarData, one per owned vehicle ID (finishes are separate entries).
    struct OwnedVehicle
    {
        uint8_t ColourIndex = 0xFF;  // 0xFF = use the vehicle list default
        uint8_t PaletteIndex = 0xFF;
        float Damage = 0.0f;         // > 0 while wrecked (won but not yet repaired)
    };

    class VehicleCatalog
    {
    public:
        // Builds the car list from the loaded vehicle list. Returns false until the game has loaded it.
        bool Build();

        const std::vector<Car>& GetCars() const { return m_Cars; }
        const Car* FindCarByVehicle(uint64_t vehicleID) const; // accepts any finish's ID

    private:
        std::vector<Car> m_Cars;
        std::unordered_map<uint64_t, size_t> m_CarIndexByVehicle;
    };

    // Profile access. All return false/defaults when the game module isn't ready.
    bool GetOwnedVehicles(std::unordered_map<uint64_t, OwnedVehicle>& owned);
    uint64_t GetChosenFinish(uint64_t carID);                 // 0 if none recorded
    void SetChosenFinish(uint64_t carID, uint64_t finishID);  // only updates an existing entry
    void SetVehicleColour(uint64_t vehicleID, uint8_t colourIndex, uint8_t paletteIndex);

    // Player's current car.
    bool GetCurrentVehicle(uint64_t& vehicleID, int& colourIndex, int& paletteIndex);
    void SetCurrentVehicleColour(int colourIndex, int paletteIndex);

    // Player car colour palettes (BrnWorld::GlobalColourPalette).
    constexpr int k_PaletteCount = 5;
    int GetPaletteColourCount(int palette);
    bool GetPaletteColour(int palette, int index, float paint[3], float pearl[3]);

    // Free-roam, offline, not in an event or challenge: the only time a Junkyard is usable.
    // Returns nullptr when allowed, else a short reason.
    const char* GetSwitchBlockedReason();

    std::string VehicleIDToString(uint64_t vehicleID);
}
