/*
 */

#ifdef __WINE__
#error wrong compiler, winegcc is not wanted for this file
#endif

#include "winesulin.h"

// --------------------------------------------------------------------------------------------------------------------
// DllMain used to get catch attach instance, so we can get module filename later

static HINSTANCE sInstance = NULL;

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID _)
{
    if (reason == DLL_PROCESS_ATTACH)
        sInstance = hInst;
    return 1;
}

// --------------------------------------------------------------------------------------------------------------------
// Plugin entry point, loads "filename.dll" + ".so" winelib and calls the entry point from there

__declspec (dllexport)
void* __cdecl VSTPluginMain(WinHostCallback winHostCallback)
{
    // safety checks first
    if (winHostCallback == NULL || winHostCallback(NULL, 1, 0, 0, NULL, 0.f) == 0)
        return NULL;

    // init only once
    static bool init = true;
    static WinVSTPluginMainFn wrapper = NULL;

    if (init)
    {
        init = false;

        // get current dll filename
        WCHAR wfilename[MAX_PATH];
        const DWORD len = GetModuleFileNameW(sInstance, wfilename, MAX_PATH);

        // ensure valid path ending with ".dll"
        if (len < 5 || len >= MAX_PATH - 4)
            return NULL;
        // TODO use strcmp similar
        if (wfilename[len-4] != '.' || wfilename[len-3] != 'd' || wfilename[len-2] != 'l' || wfilename[len-1] != 'l')
            return NULL;

        // append ".so" to wfilename
        wfilename[len + 0] = '.';
        wfilename[len + 1] = 's';
        wfilename[len + 2] = 'o';
        wfilename[len + 3] = 0;

        // load ".dll.so" wrapper
        const HANDLE h = LoadLibraryW(wfilename);

        if (h == NULL)
        {
            WCHAR* error = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL,
                           GetLastError(),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           &error, 0, NULL);
            fprintf(stderr, "[winesulin] Failed to load '%S': %S\n", wfilename, error);
            LocalFree(error);
            return NULL;
        }

        wrapper = GetProcAddress(h, "VSTPluginMain");
        fprintf(stderr, "[winesulin] Sucessfully loaded winelib '%S'\n", wfilename);
    }

    return wrapper != NULL ? wrapper(winHostCallback) : NULL;
}

// --------------------------------------------------------------------------------------------------------------------
