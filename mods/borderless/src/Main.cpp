#include <Windows.h>

#include "Borderless.hpp"


BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        Borderless::Get().Load();
        break;

    case DLL_PROCESS_DETACH:
        Borderless::Get().Unload();
        break;
    }

    return TRUE;
}
