#include <Windows.h>

#include "Controls.hpp"


BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        Controls::Get().Load();
        break;

    case DLL_PROCESS_DETACH:
        Controls::Get().Unload();
        break;
    }

    return TRUE;
}
