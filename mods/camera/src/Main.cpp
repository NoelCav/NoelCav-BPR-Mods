#include <Windows.h>

#include "Camera.hpp"


BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        Camera::Get().Load();
        break;

    case DLL_PROCESS_DETACH:
        Camera::Get().Unload();
        break;
    }

    return TRUE;
}
