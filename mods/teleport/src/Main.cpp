#include <Windows.h>

#include "Teleport.hpp"


BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        Teleport::Get().Load();
        break;

    case DLL_PROCESS_DETACH:
        Teleport::Get().Unload();
        break;
    }

    return TRUE;
}
