#include <Windows.h>

#include "Junkyard.hpp"


BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        Junkyard::Get().Load();
        break;

    case DLL_PROCESS_DETACH:
        Junkyard::Get().Unload();
        break;
    }

    return TRUE;
}
