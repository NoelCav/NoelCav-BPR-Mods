#include "Teleport.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>

#include <vector>

#include <Windows.h>
#include <Xinput.h>
#include <d3d11.h>

#include "vendor/imgui.hpp"
#include "vendor/yaml-cpp.hpp"

#include "bpr/GameEvents.hpp"

#include "core/File.hpp"
#include "core/Pointer.hpp"
#include "mod-manager/ModManager.hpp"

#include "Inflate.hpp"
#include "TeleportLocationsFile.hpp"


Teleport Teleport::s_Instance;


namespace
{
    // From the game's TRIGGERS.DAT. Each Junkyard has a drive-through trigger box
    // (GenericRegion type E_TYPE_JUNK_YARD) and two player spawns (SpawnLocation type
    // E_TYPE_PLAYER_SPAWN) just outside it, one each way, facing away from the box.
    // Junkyard CgsIDs: Angus & E. Crawford 3D34C, Hamilton & Young 476A9, Manners & S. Rouse 4C3C6,
    // Chubb Lane 46BE2, Ross & Nelson 4704A, Grange Hill C0F0D.
    struct Transform
    {
        float Position[3];
        float Direction[3];
    };

    struct Junkyard
    {
        uint32_t ID; // trigger region ID, also what the map reports when you hover it
        const char* Name;
        Transform Spawns[2];
        float BoxCenter[2]; // X, Z of the trigger box; height is taken from the spawns
    };

    constexpr Junkyard k_Junkyards[] =
    {
        { 0x3D34C, "Angus & E. Crawford", { { {  3007.97f,  -2.37f, -1968.16f }, {  0.015f, 0.009f, -1.000f } }, { {  3007.97f,  -2.37f, -1945.17f }, {  0.002f, 0.009f,  1.000f } } }, {  3007.9f, -1956.7f } },
        { 0x476A9, "Hamilton & Young",    { { {  1289.96f,  14.79f,  -495.19f }, {  0.038f, 0.000f,  0.999f } }, { {  1288.38f,  14.95f,  -523.64f }, { -0.127f, 0.000f, -0.992f } } }, {  1289.5f,  -508.5f } },
        { 0x4C3C6, "Manners & S. Rouse",  { { {  -330.44f,   8.24f,   927.12f }, { -0.993f, 0.000f,  0.117f } }, { {  -299.55f,   8.53f,   923.59f }, {  0.987f, 0.000f, -0.163f } } }, {  -320.8f,   927.9f } },
        { 0x46BE2, "Chubb Lane",          { { { -2282.15f, 102.85f,   388.73f }, { -0.999f, 0.000f, -0.046f } }, { { -2251.94f, 103.01f,   391.10f }, {  0.998f, 0.000f, -0.058f } } }, { -2268.8f,   389.2f } },
        { 0x4704A, "Ross & Nelson",       { { { -1056.99f, 113.24f, -1840.18f }, { -0.451f, 0.000f, -0.893f } }, { { -1036.89f, 113.24f, -1815.50f }, {  0.760f, 0.000f,  0.650f } } }, { -1049.5f, -1830.2f } },
        { 0xC0F0D, "Grange Hill (BSI)",   { { {  4750.36f,  61.52f,  -499.40f }, {  0.943f, 0.020f, -0.333f } }, { {  4724.90f,  61.50f,  -489.80f }, { -0.937f, 0.020f,  0.349f } } }, {  4739.6f,  -496.0f } },
    };

    // Inside the trigger box, facing through it, so the game opens the Junkyard as if you'd
    // driven in.
    Transform GetEntryTransform(const Junkyard& junkyard)
    {
        Transform entry = junkyard.Spawns[0];
        entry.Position[0] = junkyard.BoxCenter[0];
        entry.Position[1] = (junkyard.Spawns[0].Position[1] + junkyard.Spawns[1].Position[1]) / 2.0f;
        entry.Position[2] = junkyard.BoxCenter[1];
        return entry;
    }

    // BrnGui::RaceMainHudState (gm+0x7FABBC) +0x14C: the in-race HUD is up. Off while paused,
    // on the map and in a Junkyard, so it doubles as "free to drive".
    bool IsDrivingHudActive()
    {
        Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref();
        if (gameModule.GetPointer() == nullptr)
        {
            return false;
        }
        Core::Pointer raceMainHudState = gameModule.at(0x7FABBC).as<void*>();
        return raceMainHudState.GetPointer() != nullptr && raceMainHudState.at(0x14C).as<bool>();
    }

    // BurnoutPR.exe's pointer to XINPUT1_3!XInputGetState (ordinal 2); every controller read goes
    // through it. The Controls mod hooks the same slot; each hook chains to whatever was there.
    constexpr uintptr_t k_XInputGetStateSlot = 0x00CAE69C;

    using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    XInputGetStateFn s_OriginalXInputGetState = nullptr;

    bool WriteSlot(uintptr_t slot, void* value)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(slot), sizeof(void*), PAGE_READWRITE, &oldProtect))
        {
            return false;
        }
        *reinterpret_cast<void**>(slot) = value;
        VirtualProtect(reinterpret_cast<void*>(slot), sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    // Esc, for players without a controller. A scan code, so DirectInput sees it too.
    void PressEscape()
    {
        INPUT inputs[2] = {};
        for (INPUT& input : inputs)
        {
            input.type = INPUT_KEYBOARD;
            input.ki.wScan = static_cast<WORD>(MapVirtualKeyA(VK_ESCAPE, MAPVK_VK_TO_VSC));
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
        }
        inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }

    // Free-roam pause menu (any tab, map included) is open. 0 while driving, crashed, in a
    // Junkyard and in Showtime. Found 2026-10-06 by snapshot diffs of the gm block.
    constexpr ptrdiff_t k_PauseMenuOpen = 0xB6D3C6;

    // In a Junkyard (any of its screens), from driving into the box until driving out. Found
    // 2026-10-06 the same way.
    constexpr ptrdiff_t k_InJunkyard = 0xB3A939;

    // The map screen's hovered item: its ID while hovered (0 over empty ground); off the map it
    // holds small unrelated values, so only read it with the pause menu open. Found 2026-10-06
    // (the 2026-09-22 address, gm+0xB79500, is one slot of a per-icon list and only matched some
    // Junkyards).
    constexpr ptrdiff_t k_MapHoveredItem = 0x7FAD90;

    // Reads the first controller directly (the game's own XInput DLL), so this works with or
    // without the Controls mod.
    XINPUT_GAMEPAD GetController()
    {
        using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        static XInputGetStateFn xinputGetState = []() -> XInputGetStateFn
        {
            HMODULE xinput = GetModuleHandleA("XINPUT1_3.dll");
            return xinput != nullptr ? reinterpret_cast<XInputGetStateFn>(GetProcAddress(xinput, "XInputGetState")) : nullptr;
        }();

        XINPUT_STATE state = {};
        return (xinputGetState != nullptr && xinputGetState(0, &state) == ERROR_SUCCESS) ? state.Gamepad : XINPUT_GAMEPAD{};
    }

    // A Windows font close to the map's own prompt lettering (bold condensed DIN-style caps).
    std::string FindPromptFont()
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

    // The game's controller button icons: GUIAPT\B5CONTROLLERBUTTONS.BUNDLE holds one Texture
    // resource (BC3, 2048x1024). Read from the player's own install at runtime, so no game art
    // ships with the mod. Icon rectangles on the sheet, in pixels:
    constexpr float k_ButtonSheetSize[2] = { 2048.0f, 1024.0f };
    constexpr float k_ButtonA[4] = { 731.0f, 863.0f, 813.0f, 945.0f };
    constexpr float k_ButtonX[4] = { 496.0f, 904.0f, 579.0f, 987.0f };
    constexpr float k_KeyBlank[4] = { 886.0f, 589.0f, 1016.0f, 707.0f }; // key cap the game prints a letter on
    constexpr float k_KeyEnter[4] = { 619.0f, 620.0f, 726.0f, 800.0f };

    // bnd2 layout per burnout.wiki "Bundle 2/Burnout Paradise"; texture header per "Texture".
    void* LoadButtonTexture(const Core::Logger& logger)
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path = exePath;
        path = path.substr(0, path.find_last_of('\\') + 1) + "GUIAPT\\B5CONTROLLERBUTTONS.BUNDLE";

        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            logger.Warning("Button icons: can't open '%s'.", path.c_str());
            return nullptr;
        }
        std::vector<uint8_t> bundle(GetFileSize(file, nullptr));
        DWORD read = 0;
        ReadFile(file, bundle.data(), static_cast<DWORD>(bundle.size()), &read, nullptr);
        CloseHandle(file);

        auto u32 = [&](size_t offset) { return offset + 4 <= bundle.size() ? *reinterpret_cast<const uint32_t*>(&bundle[offset]) : 0u; };
        if (bundle.size() < 0x28 || std::memcmp(bundle.data(), "bnd2", 4) != 0 || (u32(0x24) & 1) == 0)
        {
            logger.Warning("Button icons: unexpected bundle format.");
            return nullptr;
        }

        uint32_t count = u32(0x10);
        uint32_t entries = u32(0x14);
        for (uint32_t i = 0; i < count; ++i)
        {
            size_t entry = entries + i * 0x40;
            if (u32(entry + 0x38) != 0) // resource type 0 = Texture
            {
                continue;
            }

            // Memory type 0 holds the texture header, type 1 the pixels; both zlib compressed.
            std::vector<uint8_t> header;
            std::vector<uint8_t> pixels;
            size_t headerAt = u32(0x18) + u32(entry + 0x28);
            size_t pixelsAt = u32(0x1C) + u32(entry + 0x2C);
            if (headerAt + u32(entry + 0x1C) > bundle.size() || pixelsAt + u32(entry + 0x20) > bundle.size() ||
                !ZlibDecompress(&bundle[headerAt], u32(entry + 0x1C), header) || header.size() < 0x28 ||
                !ZlibDecompress(&bundle[pixelsAt], u32(entry + 0x20), pixels))
            {
                logger.Warning("Button icons: failed to decompress the texture.");
                return nullptr;
            }

            DXGI_FORMAT format = static_cast<DXGI_FORMAT>(*reinterpret_cast<const uint32_t*>(&header[0x1C]));
            UINT width = *reinterpret_cast<const uint16_t*>(&header[0x24]);
            UINT height = *reinterpret_cast<const uint16_t*>(&header[0x26]);
            if (format != DXGI_FORMAT_BC3_UNORM || width != 2048 || height != 1024 || pixels.size() < width * height)
            {
                logger.Warning("Button icons: unexpected texture %ux%u format %d.", width, height, format);
                return nullptr;
            }

            ID3D11Device* device = Core::Pointer(0x01485BF8).as<ID3D11Device*>();
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data = { pixels.data(), width * 4, 0 }; // BC3: 16 bytes per 4x4 block

            ID3D11Texture2D* texture = nullptr;
            ID3D11ShaderResourceView* view = nullptr;
            if (device == nullptr || FAILED(device->CreateTexture2D(&desc, &data, &texture)) ||
                FAILED(device->CreateShaderResourceView(texture, nullptr, &view)))
            {
                logger.Warning("Button icons: failed to create the texture.");
                if (texture != nullptr)
                {
                    texture->Release();
                }
                return nullptr;
            }
            texture->Release(); // the view keeps it alive
            logger.Info("Loaded the game's button icons.");
            return view;
        }

        logger.Warning("Button icons: no texture in the bundle.");
        return nullptr;
    }

    // ImGui text squeezed horizontally, to approximate the map's condensed lettering. Returns the
    // width; with a null draw list it only measures.
    float DrawCondensedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 position, ImU32 color, const char* text)
    {
        constexpr float xScale = 0.82f;

        ImFontBaked* baked = font->GetFontBaked(size);
        if (drawList != nullptr)
        {
            drawList->PushTexture(font->OwnerAtlas->TexRef);
        }

        float x = position.x;
        for (const char* c = text; *c != '\0'; ++c)
        {
            ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(static_cast<unsigned char>(*c)));
            if (drawList != nullptr && glyph->Visible)
            {
                drawList->PrimReserve(6, 4);
                drawList->PrimRectUV(
                    ImVec2(x + glyph->X0 * xScale, position.y + glyph->Y0),
                    ImVec2(x + glyph->X1 * xScale, position.y + glyph->Y1),
                    ImVec2(glyph->U0, glyph->V0),
                    ImVec2(glyph->U1, glyph->V1),
                    color
                );
            }
            x += glyph->AdvanceX * xScale;
        }

        if (drawList != nullptr)
        {
            drawList->PopTexture();
        }
        return x - position.x;
    }

    const Junkyard* FindJunkyardByID(uint32_t id)
    {
        for (const Junkyard& junkyard : k_Junkyards)
        {
            if (junkyard.ID == id)
            {
                return &junkyard;
            }
        }
        return nullptr;
    }

    const Junkyard* FindNearestJunkyard(const float position[3])
    {
        const Junkyard* nearest = nullptr;
        float nearestDistance = 0.0f;
        for (const Junkyard& junkyard : k_Junkyards)
        {
            float dx = junkyard.BoxCenter[0] - position[0];
            float dz = junkyard.BoxCenter[1] - position[2];
            float distance = dx * dx + dz * dz;
            if (nearest == nullptr || distance < nearestDistance)
            {
                nearest = &junkyard;
                nearestDistance = distance;
            }
        }
        return nearest;
    }

}


Teleport::Teleport()
    :
    m_Logger(k_Name),
    m_ConfigDirectoryPath(ModManager::Get().GetConfigDirectoryPath().Append(k_ConfigDirectoryPath)),
    m_ConfigFilePath(Core::Path(m_ConfigDirectoryPath).Append("teleport-config.yaml")),
    m_TeleportLocationsFile(m_ConfigDirectoryPath, m_Logger)
{
}

Teleport& Teleport::Get()
{
    return s_Instance;
}

void Teleport::Load()
{
    try
    {
        if (!ModManager::Get().CheckVersion(k_Version))
        {
            throw std::exception("Mod Manager and Mod versions mismatch.");
        }

        if (!m_ConfigDirectoryPath.Exists())
        {
            m_ConfigDirectoryPath.CreateDirectoryTree();
            m_Logger.Info("Created config directory. path: '%s'", m_ConfigDirectoryPath.GetPath());
        }

        m_TeleportLocationsFile.Load();

        ModManager::Get().GetHookManager().AddGameStatePreWorldUpdateHook(
            [](Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
            {
                s_Instance.OnGameStatePreWorldUpdate(gameEventQueue, gameActionQueue);
            }
        );

        LoadConfig();
        InstallXInputHook();

        // The prompt font is added to ImGui's atlas once the game (and ImGui) is up, outside a
        // frame, like the Dashboard does.
        CloseHandle(CreateThread(nullptr, 0, [](LPVOID) -> DWORD
        {
            Core::Pointer gameModulePointer = 0x013FC8E0;
            while (gameModulePointer.as<void*>() == nullptr || gameModulePointer.deref().at(0xB6D4C8).as<int32_t>() != 6)
            {
                Sleep(1000);
            }

            s_Instance.m_ButtonTexture = LoadButtonTexture(s_Instance.m_Logger);

            std::string path = FindPromptFont();
            if (!path.empty())
            {
                EnterCriticalSection(ModManager::Get().GetImGuiManager().GetCriticalSection());
                s_Instance.m_PromptFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str());
                LeaveCriticalSection(ModManager::Get().GetImGuiManager().GetCriticalSection());
            }
            return 0;
        }, nullptr, 0, nullptr));

        ModManager::Get().GetHookManager().AddGameMainHook([]() { s_Instance.OnGameMain(); });
        ModManager::Get().GetImGuiManager().AddOverlay([]() { s_Instance.RenderMapPrompts(); });

        ModManager::Get().GetImGuiManager().AddMenu([]() { s_Instance.RenderMenu(); });
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Teleport::Unload()
{
    try
    {
        RemoveXInputHook();
        m_TeleportLocationsFile.Save();
    }
    catch (const std::exception& ex)
    {
        m_Logger.Error("%s", ex.what());
        MessageBoxA(NULL, ex.what(), k_Name, MB_ICONERROR);
    }
}

void Teleport::InstallXInputHook()
{
    // Only hook if the slot holds XInputGetState or another mod's hook chained onto it, so a
    // different game build fails safe (teleports then just wait for you to unpause).
    HMODULE xinput = GetModuleHandleA("XINPUT1_3.dll");
    void* expected = xinput != nullptr ? reinterpret_cast<void*>(GetProcAddress(xinput, MAKEINTRESOURCEA(2))) : nullptr;
    void* current = *reinterpret_cast<void**>(k_XInputGetStateSlot);
    HMODULE currentModule = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(current), &currentModule);
    bool chainedHook = currentModule != nullptr && currentModule != GetModuleHandleA(nullptr);
    if (expected == nullptr || (current != expected && !chainedHook))
    {
        m_Logger.Error("XInputGetState slot mismatch, teleports won't close the pause menu. slot: 0x%p, expected: 0x%p", current, expected);
        return;
    }

    s_OriginalXInputGetState = reinterpret_cast<XInputGetStateFn>(current);
    m_HookInstalled = WriteSlot(k_XInputGetStateSlot, reinterpret_cast<void*>(&Hook_XInputGetState));
}

void Teleport::RemoveXInputHook()
{
    // Only unhook if nothing chained on top of us since; otherwise leave the chain intact.
    if (m_HookInstalled && *reinterpret_cast<void**>(k_XInputGetStateSlot) == reinterpret_cast<void*>(&Hook_XInputGetState))
    {
        WriteSlot(k_XInputGetStateSlot, reinterpret_cast<void*>(s_OriginalXInputGetState));
    }
    m_HookInstalled = false;
}

unsigned long __stdcall Teleport::Hook_XInputGetState(unsigned long userIndex, void* state)
{
    XINPUT_STATE* xinputState = static_cast<XINPUT_STATE*>(state);
    DWORD result = s_OriginalXInputGetState(userIndex, xinputState);
    if (userIndex != 0)
    {
        return result;
    }

    s_Instance.m_ControllerConnected = result == ERROR_SUCCESS;
    if (result == ERROR_SUCCESS && s_Instance.m_PressBackFrames.load() > 0)
    {
        // Held B, as a new packet so the game treats it as fresh input.
        xinputState->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
        xinputState->dwPacketNumber ^= 0x40000000;
    }
    return result;
}

void Teleport::OnGameStatePreWorldUpdate(Core::Pointer gameEventQueue, Core::Pointer gameActionQueue)
{
    UpdateCurrentTransform();

    // A teleport queued from the map (a pause menu) runs only once the game has been back to
    // driving for a moment: on the first frames after unpausing the game ignores trigger boxes,
    // so "enter a Junkyard" wouldn't open it.
    constexpr int k_FramesBeforeTeleport = 15;
    bool driving = IsDrivingHudActive();
    m_DrivingFrames = driving ? m_DrivingFrames + 1 : 0;
    m_Driving = driving;
    if (m_DrivingFrames < k_FramesBeforeTeleport || !m_TeleportPending.exchange(false))
    {
        return;
    }

    BPR::GameEvent_TeleportPlayerVehicle gameEvent = {};
    gameEvent.Position[0] = m_PendingPosition[0];
    gameEvent.Position[1] = m_PendingPosition[1];
    gameEvent.Position[2] = m_PendingPosition[2];
    gameEvent.Position[3] = 1.0f;
    gameEvent.Direction[0] = m_PendingDirection[0];
    gameEvent.Direction[1] = m_PendingDirection[1];
    gameEvent.Direction[2] = m_PendingDirection[2];
    gameEvent.Direction[3] = 0.0f;

    BPR::GameEventQueue_AddGameEvent(
        gameEventQueue.GetPointer(),
        &gameEvent,
        BPR::GameEvent_TeleportPlayerVehicle::ID,
        sizeof(gameEvent)
    );

    m_Logger.Info(
        "Teleported to (%.2f, %.2f, %.2f).",
        m_PendingPosition[0], m_PendingPosition[1], m_PendingPosition[2]
    );
}

void Teleport::OnGameMain()
{
    // Runs every frame, including while paused on the map (the world update doesn't).
    Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref();
    if (gameModule.GetPointer() == nullptr)
    {
        return;
    }

    bool driving = IsDrivingHudActive();
    m_Driving = driving; // the world update (which also sets this) stops while paused
    bool paused = gameModule.at(k_PauseMenuOpen).as<uint8_t>() != 0;
    m_Paused = paused;
    m_InJunkyard = gameModule.at(k_InJunkyard).as<uint8_t>() != 0;

    HWND gameWindow = Core::Pointer(0x0139815C).as<HWND>();
    bool focused = GetForegroundWindow() == gameWindow;
    XINPUT_GAMEPAD pad = focused ? GetController() : XINPUT_GAMEPAD{};

    // Close the pause menu for a teleport asked for while paused. The map ignores B while A (its
    // own button) is still held, so wait until every button and our hotkeys have been let go for
    // a couple of frames. Stop pressing B as soon as we're driving, so it doesn't turn into a
    // look-back.
    if (driving)
    {
        m_PressBackFrames = 0;
    }
    else if (m_PressBackFrames.load() > 0)
    {
        --m_PressBackFrames;
    }

    bool inputHeld = pad.wButtons != 0 ||
        (focused && ((GetAsyncKeyState('2') & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0));
    m_InputReleasedFrames = inputHeld ? 0 : m_InputReleasedFrames + 1;

    if (!paused || m_InJunkyard.load() || !m_TeleportPending.load())
    {
        m_CloseMenuRequested = false;
    }
    else if (m_CloseMenuRequested.load() && m_InputReleasedFrames >= 2)
    {
        m_CloseMenuRequested = false;
        if (m_HookInstalled && m_ControllerConnected.load())
        {
            m_PressBackFrames = 4;
        }
        else
        {
            PressEscape();
        }
    }

    // Show keyboard or controller prompts for whichever was used last.
    auto stickMoved = [](SHORT value) { return value > 12000 || value < -12000; };
    if (pad.wButtons != 0 || stickMoved(pad.sThumbLX) || stickMoved(pad.sThumbLY) || stickMoved(pad.sThumbRX) || stickMoved(pad.sThumbRY))
    {
        m_UsingKeyboard = false;
    }
    else if (focused)
    {
        static constexpr int keyboardKeys[] = { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_BACK, 'W', 'A', 'S', 'D', '1', '2' };
        for (int key : keyboardKeys)
        {
            if ((GetAsyncKeyState(key) & 0x8000) != 0)
            {
                m_UsingKeyboard = true;
                break;
            }
        }
    }

    uint32_t hovered = paused ? gameModule.at(k_MapHoveredItem).as<uint32_t>() : 0;
    m_HoveredMapItem = hovered;

    const Junkyard* junkyard = m_MapTeleport.load() ? FindJunkyardByID(hovered) : nullptr;
    if (junkyard == nullptr)
    {
        // Wait for a fresh press once a Junkyard is hovered.
        m_MapKeysDown = 0xFF;
        m_MapButtonsDown = 0xFFFF;
        return;
    }

    // Keyboard: 2 = go there, Enter = go in (the map itself uses 1 and Backspace).
    uint8_t keys = 0;
    keys |= (focused && (GetAsyncKeyState('2') & 0x8000) != 0) ? 1 : 0;
    keys |= (focused && (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ? 2 : 0;
    uint8_t newKeys = keys & ~m_MapKeysDown;
    WORD newButtons = pad.wButtons & ~m_MapButtonsDown;
    m_MapKeysDown = keys;
    m_MapButtonsDown = pad.wButtons;

    bool goThere = (newButtons & XINPUT_GAMEPAD_A) != 0 || (newKeys & 1) != 0;
    bool enter = (newButtons & XINPUT_GAMEPAD_X) != 0 || (newKeys & 2) != 0;
    if (!goThere && !enter)
    {
        return;
    }

    // Queued; the map closes and it happens once the game is back to driving
    // (see OnGameStatePreWorldUpdate).
    Transform target = enter ? GetEntryTransform(*junkyard) : junkyard->Spawns[0];
    RequestTeleport(target.Position, target.Direction);
    m_Logger.Info("Map teleport to %s%s.", junkyard->Name, enter ? " (enter)" : "");
}

void Teleport::RenderMapPrompts()
{
    bool menuOpen = ImGui::GetFrameCount() - m_MenuFrame <= 1;
    if (!m_MapTeleport.load() || (FindJunkyardByID(m_HoveredMapItem.load()) == nullptr && !menuOpen))
    {
        return;
    }

    // Matches the map's own prompts: white ring with a coloured letter, then bold caps.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    void* promptFont = m_PromptFont.load();
    ImFont* font = promptFont != nullptr ? static_cast<ImFont*>(promptFont) : ImGui::GetFont();

    float unit = viewport->Size.y / 1080.0f * m_PromptScale;
    // Proportions measured from the game's own "Y Zoom Out / B Return to Game" row.
    float fontSize = 29.0f * unit;
    float radius = 19.0f * unit * m_PromptIconScale;
    float circleToText = 31.0f * unit;
    float betweenPrompts = 45.0f * unit;

    struct Prompt
    {
        const char* Button;     // letter for the drawn fallback / printed on a blank key
        ImU32 ButtonColor;
        const float* Icon;      // rectangle on the game's button sheet
        bool PrintButtonOnIcon; // blank key caps get their letter printed, like the game does
        const char* Label;
    };
    const Prompt controllerPrompts[] =
    {
        { "A", IM_COL32(110, 190, 60, 255), k_ButtonA, false, "TELEPORT" },
        { "X", IM_COL32(60, 140, 225, 255), k_ButtonX, false, "ENTER" },
    };
    const Prompt keyboardPrompts[] =
    {
        { "2", IM_COL32(255, 255, 255, 255), k_KeyBlank, true, "TELEPORT" },
        { "", IM_COL32(255, 255, 255, 255), k_KeyEnter, false, "ENTER" },
    };
    bool keyboard = m_PromptIcons == 2 || (m_PromptIcons == 0 && m_UsingKeyboard.load());
    const Prompt* prompts = keyboard ? keyboardPrompts : controllerPrompts;
    void* buttonTexture = m_ButtonTexture.load();

    // Icons keep the sheet's aspect ratio at the row's icon height.
    auto iconWidth = [&](const Prompt& prompt)
    {
        return buttonTexture != nullptr ? radius * 2.0f * (prompt.Icon[2] - prompt.Icon[0]) / (prompt.Icon[3] - prompt.Icon[1]) : radius * 2.0f;
    };

    float totalWidth = -betweenPrompts;
    for (int i = 0; i < 2; ++i)
    {
        totalWidth += iconWidth(prompts[i]) + circleToText + DrawCondensedText(nullptr, font, fontSize, ImVec2(), 0, prompts[i].Label) + betweenPrompts;
    }

    float x = viewport->Pos.x + viewport->Size.x / 2.0f + m_PromptX * unit / m_PromptScale - totalWidth;
    float y = viewport->Pos.y + viewport->Size.y - m_PromptY * unit / m_PromptScale;

    // Draw text vertically centered on the row by its capital letters, a hair doubled to
    // thicken Bahnschrift's regular weight towards the game's bold.
    auto drawCentered = [&](float left, float size, ImU32 color, const char* text)
    {
        ImFontGlyph* capital = font->GetFontBaked(size)->FindGlyph('E');
        float top = y - (capital->Y0 + capital->Y1) / 2.0f;
        float shadow = 2.0f * unit;
        DrawCondensedText(drawList, font, size, ImVec2(left + shadow, top + shadow), IM_COL32(0, 0, 0, 170), text);
        DrawCondensedText(drawList, font, size, ImVec2(left + shadow + 0.7f * unit, top + shadow), IM_COL32(0, 0, 0, 170), text);
        DrawCondensedText(drawList, font, size, ImVec2(left, top), color, text);
        return DrawCondensedText(drawList, font, size, ImVec2(left + 0.7f * unit, top), color, text);
    };

    for (int i = 0; i < 2; ++i)
    {
        const Prompt& prompt = prompts[i];
        float width = iconWidth(prompt);
        ImVec2 center = ImVec2(x + width / 2.0f, y);
        if (buttonTexture != nullptr)
        {
            // The game's own icon.
            drawList->AddImage(
                reinterpret_cast<ImTextureID>(buttonTexture),
                ImVec2(center.x - width / 2.0f, center.y - radius),
                ImVec2(center.x + width / 2.0f, center.y + radius),
                ImVec2(prompt.Icon[0] / k_ButtonSheetSize[0], prompt.Icon[1] / k_ButtonSheetSize[1]),
                ImVec2(prompt.Icon[2] / k_ButtonSheetSize[0], prompt.Icon[3] / k_ButtonSheetSize[1])
            );
            if (prompt.PrintButtonOnIcon)
            {
                float letterSize = fontSize * 0.9f;
                float letterWidth = DrawCondensedText(nullptr, font, letterSize, ImVec2(), 0, prompt.Button);
                drawCentered(center.x - letterWidth / 2.0f, letterSize, prompt.ButtonColor, prompt.Button);
            }
        }
        else
        {
            // Fallback: a drawn look-alike.
            drawList->AddCircleFilled(center, radius, IM_COL32(20, 20, 20, 160), 40);
            drawList->AddCircle(center, radius - 1.5f * unit, IM_COL32(255, 255, 255, 255), 40, 3.0f * unit);

            float letterSize = fontSize * 0.9f;
            float letterWidth = DrawCondensedText(nullptr, font, letterSize, ImVec2(), 0, prompt.Button);
            drawCentered(center.x - letterWidth / 2.0f, letterSize, prompt.ButtonColor, prompt.Button);
        }

        x += width + circleToText;
        x += drawCentered(x, fontSize, IM_COL32(255, 255, 255, 255), prompt.Label) + betweenPrompts;
    }
}

void Teleport::LoadConfig()
{
    try
    {
        YAML::Node yaml = YAML::Load(Core::File(m_ConfigFilePath, Core::File::Mode::Read, m_Logger).ReadAsText());
        m_PromptX = yaml["MapPromptX"].as<float>(m_PromptX);
        m_PromptY = yaml["MapPromptY"].as<float>(m_PromptY);
        m_PromptScale = yaml["MapPromptScale"].as<float>(m_PromptScale);
        m_PromptIconScale = yaml["MapPromptIconScale"].as<float>(m_PromptIconScale);
        m_PromptIcons = yaml["MapPromptIcons"].as<int>(m_PromptIcons);
        m_MapTeleport = yaml["MapTeleport"].as<bool>(m_MapTeleport.load());
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to load teleport config, using defaults. exception: %s", ex.what());
    }
}

void Teleport::SaveConfig() const
{
    try
    {
        YAML::Node yaml;
        yaml["MapPromptX"] = m_PromptX;
        yaml["MapPromptY"] = m_PromptY;
        yaml["MapPromptScale"] = m_PromptScale;
        yaml["MapPromptIconScale"] = m_PromptIconScale;
        yaml["MapPromptIcons"] = m_PromptIcons;
        yaml["MapTeleport"] = m_MapTeleport.load();
        Core::File(m_ConfigFilePath, Core::File::Mode::Write, m_Logger).WriteAsText(YAML::Dump(yaml));
    }
    catch (const std::exception& ex)
    {
        m_Logger.Warning("Failed to save teleport config. exception: %s", ex.what());
    }
}

void Teleport::UpdateCurrentTransform()
{
    // BrnGameModule -> ActiveRaceCar[playerIndex] -> RaceCar*. The RaceCar starts with the
    // car's world matrix: rows right (+0x00), up (+0x10), forward (+0x20), translation (+0x30).
    bool valid = false;
    float position[3] = {};
    float direction[3] = {};

    Core::Pointer gameModule = Core::Pointer(0x013FC8E0).deref();
    if (gameModule.GetPointer() != nullptr)
    {
        uint32_t playerIndex = gameModule.at(0x40C28).as<uint32_t>();
        if (playerIndex < 8)
        {
            Core::Pointer raceCar = gameModule.at(0x12980 + playerIndex * 0x4180).at(0x7C0).deref();
            if (raceCar.GetPointer() != nullptr)
            {
                const float* matrix = raceCar.GetPointer<const float*>();
                std::memcpy(direction, matrix + 8, sizeof(direction));
                std::memcpy(position, matrix + 12, sizeof(position));
                valid = true;
            }
        }
    }

    std::scoped_lock lock(m_CurrentTransformMutex);
    m_CurrentTransformValid = valid;
    std::memcpy(m_CurrentPosition, position, sizeof(m_CurrentPosition));
    std::memcpy(m_CurrentDirection, direction, sizeof(m_CurrentDirection));
}

bool Teleport::GetCurrentTransform(float position[3], float direction[3])
{
    std::scoped_lock lock(m_CurrentTransformMutex);
    std::memcpy(position, m_CurrentPosition, sizeof(m_CurrentPosition));
    std::memcpy(direction, m_CurrentDirection, sizeof(m_CurrentDirection));
    return m_CurrentTransformValid;
}

void Teleport::RequestTeleport(const float position[3], const float direction[3])
{
    std::memcpy(m_PendingPosition, position, sizeof(m_PendingPosition));
    std::memcpy(m_PendingDirection, direction, sizeof(m_PendingDirection));
    m_TeleportPending.store(true);
    m_CloseMenuRequested = m_Paused.load(); // closed once the button that asked is released
}

void Teleport::RenderMenu()
{
    ImGui::SetNextWindowPos(ImVec2(620.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(680.0f, 900.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(k_Name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() / 2.0f);

        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Version     %s", k_Version);
        ImGui::Text("Author      %s", k_Author);
        ImGui::Text("Framerate   %.1f", io.Framerate);

        ImGui::Separator();
        float currentPosition[3] = {};
        float currentDirection[3] = {};
        bool hasCurrentTransform = GetCurrentTransform(currentPosition, currentDirection);
        if (hasCurrentTransform)
        {
            ImGui::Text("Current     %.2f  %.2f  %.2f", currentPosition[0], currentPosition[1], currentPosition[2]);

            // Presets always save the car's live transform, never the manual fields below.
            ImGui::InputText("Name", m_NewLocationName, sizeof(m_NewLocationName));

            ImGui::SameLine();
            if (ImGui::Button("Save current position") && m_NewLocationName[0] != '\0')
            {
                TeleportLocationsFile::Location location;
                location.Name = m_NewLocationName;
                std::memcpy(location.Position, currentPosition, sizeof(location.Position));
                std::memcpy(location.Direction, currentDirection, sizeof(location.Direction));
                m_TeleportLocationsFile.AddLocation(location);
                m_TeleportLocationsFile.Save();
                m_NewLocationName[0] = '\0';
            }

            if (ImGui::Button("Copy to manual fields"))
            {
                std::memcpy(m_InputPosition, currentPosition, sizeof(m_InputPosition));
                std::memcpy(m_InputDirection, currentDirection, sizeof(m_InputDirection));
            }
        }
        else
        {
            ImGui::Text("Current     (no player vehicle)");
        }

        // Teleporting from inside a Junkyard breaks the camera. From a pause menu the teleport
        // closes the menu and happens once back on the road.
        bool canTeleport = !m_InJunkyard.load();
        if (!canTeleport)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.3f, 1.0f), "Teleports are off in Junkyards.");
        }
        else if (m_Paused.load())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.3f, 1.0f), "Paused: teleporting closes the pause menu.");
        }
        ImGui::BeginDisabled(!canTeleport);

        ImGui::Separator();
        ImGui::Text("Manual teleport");
        ImGui::InputFloat3("Position", m_InputPosition);
        ImGui::InputFloat3("Direction", m_InputDirection);

        if (ImGui::Button("Teleport"))
        {
            RequestTeleport(m_InputPosition, m_InputDirection);
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Junkyards", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const Junkyard* nearest = hasCurrentTransform ? FindNearestJunkyard(currentPosition) : nullptr;

            ImGui::BeginDisabled(nearest == nullptr);
            if (ImGui::Button("Nearest Junkyard") && nearest != nullptr)
            {
                RequestTeleport(nearest->Spawns[0].Position, nearest->Spawns[0].Direction);
            }
            ImGui::SameLine();
            if (ImGui::Button("Enter nearest Junkyard") && nearest != nullptr)
            {
                Transform entry = GetEntryTransform(*nearest);
                RequestTeleport(entry.Position, entry.Direction);
            }
            ImGui::EndDisabled();
            if (nearest != nullptr)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", nearest->Name);
            }

            bool mapTeleport = m_MapTeleport.load();
            if (ImGui::Checkbox("Map teleport (buttons on the map)", &mapTeleport))
            {
                m_MapTeleport = mapTeleport;
                SaveConfig();
            }
            if (mapTeleport)
            {
                ImGui::TextWrapped("On the map: hover a Junkyard and press A (keyboard 2) to go there, or X (keyboard Enter) to go in. The map closes and you're teleported.");
                const Junkyard* hoveredJunkyard = FindJunkyardByID(m_HoveredMapItem.load());
                if (hoveredJunkyard != nullptr)
                {
                    ImGui::TextDisabled("Map cursor: %s", hoveredJunkyard->Name);
                }

                // The prompts are previewed on screen while this window is open.
                m_MenuFrame = ImGui::GetFrameCount();
                if (ImGui::TreeNode("Map prompt position"))
                {
                    bool changed = false;
                    ImGui::DragFloat("Right edge (from center)", &m_PromptX, 1.0f, -960.0f, 960.0f, "%.0f");
                    changed |= ImGui::IsItemDeactivatedAfterEdit();
                    ImGui::DragFloat("Height (from bottom)", &m_PromptY, 1.0f, 0.0f, 1080.0f, "%.0f");
                    changed |= ImGui::IsItemDeactivatedAfterEdit();
                    ImGui::SliderFloat("Size", &m_PromptScale, 0.5f, 2.0f, "%.2f");
                    changed |= ImGui::IsItemDeactivatedAfterEdit();
                    ImGui::SliderFloat("Icon size", &m_PromptIconScale, 0.5f, 2.5f, "%.2f");
                    changed |= ImGui::IsItemDeactivatedAfterEdit();
                    static constexpr const char* iconSets[] = { "Auto (last used)", "Controller", "Keyboard" };
                    changed |= ImGui::Combo("Icons", &m_PromptIcons, iconSets, IM_ARRAYSIZE(iconSets));
                    if (changed)
                    {
                        SaveConfig();
                    }
                    ImGui::TreePop();
                }
            }

            if (ImGui::BeginTable("##junkyards", 4, ImGuiTableFlags_SizingStretchProp))
            {
                for (const Junkyard& junkyard : k_Junkyards)
                {
                    ImGui::PushID(junkyard.Name);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(junkyard.Name);

                    for (int side = 0; side < 2; ++side)
                    {
                        ImGui::TableSetColumnIndex(1 + side);
                        ImGui::PushID(side);
                        if (ImGui::Button(side == 0 ? "Outside, side 1" : "Outside, side 2"))
                        {
                            RequestTeleport(junkyard.Spawns[side].Position, junkyard.Spawns[side].Direction);
                        }
                        ImGui::PopID();
                    }

                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::Button("Enter"))
                    {
                        Transform entry = GetEntryTransform(junkyard);
                        RequestTeleport(entry.Position, entry.Direction);
                    }

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Separator();
        ImGui::Text("Presets");

        std::string locationToRemove;
        bool locationUpdated = false;
        for (TeleportLocationsFile::Location& location : m_TeleportLocationsFile.GetLocations())
        {
            ImGui::PushID(location.Name.c_str());

            ImGui::Text("%s", location.Name.c_str());

            ImGui::SameLine();
            if (ImGui::Button("Teleport"))
            {
                RequestTeleport(location.Position, location.Direction);
            }

            // Overwrite the preset with the car's live position + direction.
            ImGui::SameLine();
            ImGui::BeginDisabled(!hasCurrentTransform);
            if (ImGui::Button("Update"))
            {
                std::memcpy(location.Position, currentPosition, sizeof(location.Position));
                std::memcpy(location.Direction, currentDirection, sizeof(location.Direction));
                locationUpdated = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Delete"))
            {
                locationToRemove = location.Name;
            }

            ImGui::PopID();
        }

        if (locationUpdated)
        {
            m_TeleportLocationsFile.Save();
        }

        if (!locationToRemove.empty())
        {
            m_TeleportLocationsFile.RemoveLocationByName(locationToRemove);
            m_TeleportLocationsFile.Save();
        }

        ImGui::EndDisabled();

        ImGui::PopItemWidth();
    }
    ImGui::End();
}
