#include "Junkyard.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include <Windows.h>

#include "vendor/imgui.hpp"

#include "bpr/GameEvents.hpp"

#include "mod-manager/ModManager.hpp"


Junkyard Junkyard::s_Instance;


namespace
{
    struct CategoryTab
    {
        uint32_t Category;
        const char* Label;
    };

    constexpr CategoryTab k_CategoryTabs[] =
    {
        { GameData::Category_ParadiseCars,  "Paradise Cars" },
        { GameData::Category_ParadiseBikes, "Bikes" },
        { GameData::Category_OnlineCars,    "Online Cars" },
        { GameData::Category_Toys,          "Toy Cars" },
        { GameData::Category_Legendary,     "Legendary Cars" },
        { GameData::Category_BoostSpecials, "Boost Specials" },
        { GameData::Category_Cops,          "Cop Cars" },
        { GameData::Category_BigSurf,       "Big Surf Island" },
    };

    // The Junkyard's paint shop offers these three; Special (gold/platinum) and Party are
    // assigned by the game, so they're shown but not offered.
    constexpr const char* k_PaletteNames[GameData::k_PaletteCount] = { "Gloss", "Metallic", "Pearlescent", "Special", "Party" };
    constexpr int k_PaintablePalettes = 3;

    constexpr ImVec4 k_WarningColour = ImVec4(1.0f, 0.65f, 0.3f, 1.0f);
    constexpr ImVec4 k_GoodColour = ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
    constexpr ImVec4 k_DimColour = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    constexpr ImVec4 k_RewardColour = ImVec4(1.0f, 0.5f, 0.1f, 1.0f);

    const char* GetBoostName(const GameData::Car& car)
    {
        switch (car.BoostType)
        {
        case 0: return "Speed";
        case 1: return "Aggression";
        case 2: return "Stunt";
        }
        return "-";
    }

    const GameData::Finish* FindFinish(const GameData::Car& car, uint64_t vehicleID)
    {
        for (const GameData::Finish& finish : car.Finishes)
        {
            if (finish.ID == vehicleID)
            {
                return &finish;
            }
        }
        return nullptr;
    }
}


Junkyard::Junkyard()
    :
    m_Logger(k_Name)
{
}

Junkyard& Junkyard::Get()
{
    return s_Instance;
}

void Junkyard::Load()
{
    try
    {
        if (!ModManager::Get().CheckVersion(k_Version))
        {
            throw std::exception("Mod Manager and Mod versions mismatch.");
        }

        ModManager::Get().GetHookManager().AddGameStatePreWorldUpdateHook(
            [](Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
            {
                s_Instance.OnGameStatePreWorldUpdate(gameEventQueue, gameActionQueue);
            }
        );

        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Junkyard::Unload()
{
}

void Junkyard::OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
{
    // Building the list calls into the game (localized names, manufacturers), so do it here on
    // the game thread rather than from the menu.
    if (!m_CatalogReady.load())
    {
        uint64_t vehicleID = 0;
        int colour = 0;
        int palette = 0;
        if (GameData::GetCurrentVehicle(vehicleID, colour, palette) && m_Catalog.Build())
        {
            m_CatalogReady = true;
        }
        return;
    }

    uint64_t carID = 0;
    uint64_t vehicleID = 0;
    PendingPaint paint;
    {
        std::scoped_lock lock(m_PendingMutex);
        carID = m_PendingCarID;
        vehicleID = m_PendingVehicleID;
        paint = m_PendingPaint;
        m_PendingCarID = 0;
        m_PendingVehicleID = 0;
        m_PendingPaint = {};
    }

    uint64_t currentVehicleID = 0;
    int currentColour = 0;
    int currentPalette = 0;
    bool hasCurrentVehicle = GameData::GetCurrentVehicle(currentVehicleID, currentColour, currentPalette);

    if (paint.Valid)
    {
        GameData::SetVehicleColour(paint.VehicleID, static_cast<uint8_t>(paint.ColourIndex), static_cast<uint8_t>(paint.PaletteIndex));
        if (paint.IsCurrentVehicle && hasCurrentVehicle && currentVehicleID == paint.VehicleID)
        {
            GameData::SetCurrentVehicleColour(paint.ColourIndex, paint.PaletteIndex);
        }
    }

    if (vehicleID != 0)
    {
        const char* blockedReason = GameData::GetSwitchBlockedReason();
        if (blockedReason != nullptr)
        {
            m_Logger.Warning("Car change dropped: %s", blockedReason);
        }
        else
        {
            // Record the finish the way the real Junkyard does, so it's remembered next time.
            // The change itself is the same GameEvent_ChangePlayerVehicle matty-ross's mod-menu used.
            GameData::SetChosenFinish(carID, vehicleID);

            BPR::GameEvent_ChangePlayerVehicle gameEvent =
            {
                .VehicleID         = vehicleID,
                .WheelID           = 0, // the vehicle's own default wheels
                .ResetPlayerCamera = true,
                .KeepResetSection  = true,
            };
            BPR::GameEventQueue_AddGameEvent(gameEventQueue.GetPointer(), &gameEvent, gameEvent.ID, sizeof(gameEvent));

            m_RestorePaintForVehicleID = vehicleID;
            m_RestorePaintFrames = 300;
            m_Logger.Info("Changing car to %s.", GameData::VehicleIDToString(vehicleID).c_str());
        }
    }

    // Once the new car is spawned, give it the paint saved for it in the profile.
    if (m_RestorePaintForVehicleID != 0)
    {
        if (hasCurrentVehicle && currentVehicleID == m_RestorePaintForVehicleID)
        {
            std::unordered_map<uint64_t, GameData::OwnedVehicle> owned;
            const GameData::Car* car = m_Catalog.FindCarByVehicle(currentVehicleID);
            const GameData::Finish* finish = car != nullptr ? FindFinish(*car, currentVehicleID) : nullptr;
            if (finish != nullptr && GameData::GetOwnedVehicles(owned))
            {
                auto it = owned.find(currentVehicleID);
                if (it != owned.end())
                {
                    bool hasSaved = it->second.ColourIndex != 0xFF && it->second.PaletteIndex != 0xFF;
                    GameData::SetCurrentVehicleColour(
                        hasSaved ? it->second.ColourIndex : finish->DefaultColourIndex,
                        hasSaved ? it->second.PaletteIndex : finish->DefaultPaletteIndex
                    );
                }
            }
            m_RestorePaintForVehicleID = 0;
        }
        else if (--m_RestorePaintFrames <= 0)
        {
            m_RestorePaintForVehicleID = 0;
        }
    }
}

uint64_t Junkyard::GetDisplayedFinish(const GameData::Car& car) const
{
    auto isOwned = [&](uint64_t id) { return id != 0 && m_Owned.contains(id) && FindFinish(car, id) != nullptr; };

    if (isOwned(m_SelectedFinishID))
    {
        return m_SelectedFinishID;
    }

    uint64_t chosen = GameData::GetChosenFinish(car.ID);
    return isOwned(chosen) ? chosen : car.ID;
}

void Junkyard::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(40.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720.0f, 820.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);

        uint64_t currentVehicleID = 0;
        int currentColour = 0;
        int currentPalette = 0;
        GameData::GetCurrentVehicle(currentVehicleID, currentColour, currentPalette);

        if (!m_CatalogReady.load())
        {
            ImGui::TextColored(k_DimColour, "Waiting for the game to load its vehicle list...");
            ImGui::End();
            return;
        }

        if (!GameData::GetOwnedVehicles(m_Owned))
        {
            ImGui::TextColored(k_DimColour, "Waiting for your profile to load...");
            ImGui::End();
            return;
        }

        const GameData::Car* currentCar = m_Catalog.FindCarByVehicle(currentVehicleID);
        const GameData::Finish* currentFinish = currentCar != nullptr ? FindFinish(*currentCar, currentVehicleID) : nullptr;
        if (currentCar != nullptr)
        {
            ImGui::Text("Driving     %s (%s)", currentCar->Name.c_str(), currentFinish != nullptr ? currentFinish->Name.c_str() : "?");
        }

        const char* blockedReason = GameData::GetSwitchBlockedReason();
        if (blockedReason != nullptr)
        {
            ImGui::TextColored(k_WarningColour, "%s", blockedReason);
        }

        ImGui::Separator();

        if (ImGui::BeginTabBar("##categories"))
        {
            for (const CategoryTab& tab : k_CategoryTabs)
            {
                int ownedCount = 0;
                for (const GameData::Car& car : m_Catalog.GetCars())
                {
                    ownedCount += ((car.Categories & tab.Category) != 0 && m_Owned.contains(car.ID)) ? 1 : 0;
                }
                if (ownedCount == 0)
                {
                    continue;
                }

                std::string label = std::string(tab.Label) + " (" + std::to_string(ownedCount) + ")###" + tab.Label;
                if (ImGui::BeginTabItem(label.c_str()))
                {
                    RenderCarTable(tab.Category, currentCar != nullptr ? currentCar->ID : 0);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        const GameData::Car* selected = m_Catalog.FindCarByVehicle(m_SelectedCarID);
        if (selected == nullptr && currentCar != nullptr)
        {
            selected = currentCar;
            m_SelectedCarID = currentCar->ID;
        }
        if (selected != nullptr)
        {
            RenderSelectedCar(*selected, currentVehicleID, blockedReason);
        }
    }
    ImGui::End();
}

void Junkyard::RenderCarTable(uint32_t category, uint64_t currentCarID)
{
    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;

    float height = ImGui::GetContentRegionAvail().y * 0.5f;
    if (!ImGui::BeginTable("##cars", 7, tableFlags, ImVec2(0.0f, height)))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    // Click a header to sort; with no sort (the default) cars are in game order.
    ImGui::TableSetupColumn("Car", ImGuiTableColumnFlags_WidthStretch, 3.2f);
    ImGui::TableSetupColumn("Boost", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Spd", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.4f);
    ImGui::TableSetupColumn("Bst", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.4f);
    ImGui::TableSetupColumn("Str", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.4f);
    ImGui::TableSetupColumn("Burning Route", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 1.1f);
    ImGui::TableSetupColumn("Condition", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 0.9f);
    ImGui::TableHeadersRow();

    int sortColumn = -1;
    bool sortDescending = false;
    if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs != nullptr && sortSpecs->SpecsCount > 0)
    {
        sortColumn = sortSpecs->Specs[0].ColumnIndex;
        sortDescending = sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }

    auto isListed = [&](const GameData::Car& car)
    {
        return (car.Categories & category) != 0 && m_Owned.contains(car.ID);
    };

    auto renderRow = [&](const GameData::Car& car, bool isReward)
    {
        ImGui::PushID(&car);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        std::string label = (car.ID == currentCarID ? "> " : "") + car.Name;
        if (ImGui::Selectable(label.c_str(), car.ID == m_SelectedCarID, ImGuiSelectableFlags_SpanAllColumns))
        {
            m_SelectedCarID = car.ID;
            m_SelectedFinishID = 0;
        }
        if (isReward)
        {
            // Burning Route reward: tagged after its name.
            ImGui::SameLine();
            ImGui::TextColored(k_RewardColour, "BR");
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(GetBoostName(car));
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%d", car.SpeedStat);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", car.BoostStat);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%d", car.StrengthStat);

        ImGui::TableSetColumnIndex(5);
        if (isReward)
        {
            ImGui::TextColored(k_RewardColour, "Reward");
        }
        else if (car.BurningRouteCarID == 0)
        {
            ImGui::TextColored(k_DimColour, "-");
        }
        else if (m_Owned.contains(car.BurningRouteCarID))
        {
            ImGui::TextColored(k_GoodColour, "Done");
        }
        else
        {
            ImGui::TextUnformatted("Not done");
        }

        ImGui::TableSetColumnIndex(6);
        if (m_Owned.at(car.ID).Damage > 0.0f)
        {
            ImGui::TextColored(k_WarningColour, "Wrecked");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Won but not repaired yet: drive it through an Auto Repair.");
            }
        }
        else
        {
            ImGui::TextColored(k_DimColour, "OK");
        }

        ImGui::PopID();
    };

    // Every car is its own row, Burning Route rewards included, so sorting can move them freely.
    std::vector<const GameData::Car*> cars;
    for (const GameData::Car& car : m_Catalog.GetCars())
    {
        if (isListed(car))
        {
            cars.push_back(&car);
        }
    }

    auto key = [sortColumn](const GameData::Car* car) -> int
    {
        switch (sortColumn)
        {
        case 1: return car->BoostType;
        case 2: return car->SpeedStat;
        case 3: return car->BoostStat;
        case 4: return car->StrengthStat;
        }
        return 0;
    };
    // Stable, so ties (and the Car column) stay in game order.
    if (sortColumn > 0)
    {
        std::stable_sort(cars.begin(), cars.end(), [&](const GameData::Car* a, const GameData::Car* b)
        {
            return sortDescending ? key(a) > key(b) : key(a) < key(b);
        });
    }
    else if (sortColumn == 0 && sortDescending)
    {
        std::reverse(cars.begin(), cars.end());
    }

    for (const GameData::Car* car : cars)
    {
        renderRow(*car, car->RewardForCarID != 0);
    }

    ImGui::EndTable();
}

void Junkyard::RenderSelectedCar(const GameData::Car& car, uint64_t currentVehicleID, const char* blockedReason)
{
    ImGui::SeparatorText(car.Name.c_str());

    if (car.RewardForCarID != 0)
    {
        const GameData::Car* base = m_Catalog.FindCarByVehicle(car.RewardForCarID);
        ImGui::TextColored(k_RewardColour, "Burning Route reward for the %s", base != nullptr ? base->Name.c_str() : "?");
    }

    auto renderStat = [](const char* label, int value)
    {
        char overlay[8] = {};
        sprintf_s(overlay, "%d", value);
        ImGui::ProgressBar(std::clamp(value, 0, 10) / 10.0f, ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0.0f), overlay);
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };
    renderStat("Speed", car.SpeedStat);
    renderStat("Boost", car.BoostStat);
    renderStat("Strength", car.StrengthStat);

    uint64_t finishID = GetDisplayedFinish(car);
    const GameData::Finish* finish = FindFinish(car, finishID);
    if (finish == nullptr)
    {
        return;
    }

    // Finish picker: only finishes you've unlocked.
    if (ImGui::BeginCombo("Finish", finish->Name.c_str()))
    {
        for (const GameData::Finish& option : car.Finishes)
        {
            if (!m_Owned.contains(option.ID))
            {
                continue;
            }
            if (ImGui::Selectable(option.Name.c_str(), option.ID == finishID))
            {
                m_SelectedFinishID = option.ID;
                finishID = option.ID;
                finish = &option;
            }
        }
        ImGui::EndCombo();
    }

    bool isCurrentVehicle = finishID == currentVehicleID;

    ImGui::BeginDisabled(blockedReason != nullptr || isCurrentVehicle);
    if (ImGui::Button(isCurrentVehicle ? "Driving this car" : "Drive this car"))
    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingCarID = car.ID;
        m_PendingVehicleID = finishID;
    }
    ImGui::EndDisabled();

    auto owned = m_Owned.find(finishID);
    if (owned != m_Owned.end() && owned->second.Damage > 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextColored(k_WarningColour, "Wrecked - drive it through an Auto Repair to fix it.");
    }

    RenderPaint(car, *finish, isCurrentVehicle);
}

void Junkyard::RenderPaint(const GameData::Car& car, const GameData::Finish& finish, bool isCurrentVehicle)
{
    ImGui::SeparatorText("Paint");

    if (!car.Paintable)
    {
        ImGui::TextColored(k_DimColour, "This car can't be repainted.");
        return;
    }

    int colourIndex = finish.DefaultColourIndex;
    int paletteIndex = finish.DefaultPaletteIndex;
    if (isCurrentVehicle)
    {
        uint64_t vehicleID = 0;
        GameData::GetCurrentVehicle(vehicleID, colourIndex, paletteIndex);
    }
    else
    {
        auto owned = m_Owned.find(finish.ID);
        if (owned != m_Owned.end() && owned->second.ColourIndex != 0xFF && owned->second.PaletteIndex != 0xFF)
        {
            colourIndex = owned->second.ColourIndex;
            paletteIndex = owned->second.PaletteIndex;
        }
    }

    float paint[3] = {};
    float pearl[3] = {};
    if (GameData::GetPaletteColour(paletteIndex, colourIndex, paint, pearl))
    {
        ImGui::ColorButton("##current-paint", ImVec4(paint[0], paint[1], paint[2], 1.0f), ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
        ImGui::ColorButton("##current-pearl", ImVec4(pearl[0], pearl[1], pearl[2], 1.0f), ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
    }
    ImGui::Text(
        "%s %d%s",
        (paletteIndex >= 0 && paletteIndex < GameData::k_PaletteCount) ? k_PaletteNames[paletteIndex] : "?",
        colourIndex + 1,
        isCurrentVehicle ? "" : "  (applies when you drive it)"
    );

    if (!ImGui::BeginTabBar("##palettes"))
    {
        return;
    }

    for (int palette = 0; palette < k_PaintablePalettes; ++palette)
    {
        if (!ImGui::BeginTabItem(k_PaletteNames[palette]))
        {
            continue;
        }

        int count = GameData::GetPaletteColourCount(palette);
        constexpr int perRow = 9;
        for (int i = 0; i < count; ++i)
        {
            if (!GameData::GetPaletteColour(palette, i, paint, pearl))
            {
                continue;
            }

            ImGui::PushID(i);
            if (i % perRow != 0)
            {
                ImGui::SameLine();
            }

            bool selected = palette == paletteIndex && i == colourIndex;
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
            }

            char tooltip[32] = {};
            sprintf_s(tooltip, "%s %d", k_PaletteNames[palette], i + 1);
            if (ImGui::ColorButton(tooltip, ImVec4(paint[0], paint[1], paint[2], 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(40.0f, 40.0f)))
            {
                std::scoped_lock lock(m_PendingMutex);
                m_PendingPaint = PendingPaint
                {
                    .Valid = true,
                    .VehicleID = finish.ID,
                    .IsCurrentVehicle = isCurrentVehicle,
                    .ColourIndex = i,
                    .PaletteIndex = palette,
                };
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(tooltip);
                ImGui::ColorButton("##pearl", ImVec4(pearl[0], pearl[1], pearl[2], 1.0f), ImGuiColorEditFlags_NoTooltip);
                ImGui::SameLine();
                ImGui::TextUnformatted("pearl");
                ImGui::EndTooltip();
            }

            if (selected)
            {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}
