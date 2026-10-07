#pragma once


#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "core/Logger.hpp"
#include "core/Pointer.hpp"

#include "GameData.hpp"


// Portable Junkyard: pick any car you own, in the game's order and category tabs, pick one of
// its unlocked finishes, and paint it - from anywhere in free roam. Only what a real Junkyard
// would let you do; nothing is unlocked or repaired for you.
// Inspired by the portable Junkyard in Brick Remastered (by Brick); no code from it is used.
class Junkyard
{
private:
    Junkyard();

public:
    static Junkyard& Get();

public:
    void Load();
    void Unload();

private:
    void OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue);

    void RenderMenu();
    void RenderCarTable(uint32_t category, uint64_t currentCarID);
    void RenderSelectedCar(const GameData::Car& car, uint64_t currentVehicleID, const char* blockedReason);
    void RenderPaint(const GameData::Car& car, const GameData::Finish& finish, bool isCurrentVehicle);

    uint64_t GetDisplayedFinish(const GameData::Car& car) const;

private:
    static constexpr char k_Name[] = "Junkyard";
    static constexpr char k_Version[] = "2.0.0"; // must match ModManager::k_Version
    static constexpr char k_Author[] = "NoelCav";

    static Junkyard s_Instance;

private:
    Core::Logger m_Logger;

    GameData::VehicleCatalog m_Catalog;   // built once on the game thread, then read-only
    std::atomic<bool> m_CatalogReady = false;
    std::unordered_map<uint64_t, GameData::OwnedVehicle> m_Owned; // refreshed every menu frame

    uint64_t m_SelectedCarID = 0;
    uint64_t m_SelectedFinishID = 0;

    // Menu -> game thread.
    struct PendingPaint
    {
        bool Valid = false;
        uint64_t VehicleID = 0;
        bool IsCurrentVehicle = false;
        int ColourIndex = 0;
        int PaletteIndex = 0;
    };

    std::mutex m_PendingMutex;
    uint64_t m_PendingCarID = 0;     // car (primary finish) whose chosen finish to record
    uint64_t m_PendingVehicleID = 0; // finish to drive, 0 = none
    PendingPaint m_PendingPaint;

    // After a car change, re-apply the car's saved paint once the new car is in.
    uint64_t m_RestorePaintForVehicleID = 0;
    int m_RestorePaintFrames = 0;
};
