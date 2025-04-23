/*
 */

#ifndef __WINE__
#error wrong compiler or wrong winegcc flags
#endif

#include "winesulin.h"

#include <dlfcn.h>

#define WINESULIN_CLASS_NAME "WineSuLin"
#define WINESULIN_EFFECT_PROP_NAME "WineSuLinFxPtr"
// #define WINESULIN_GUI_IS_EMBED
#define WINESULIN_GUI_IS_SUPPORTED

#ifdef WINESULIN_GUI_IS_SUPPORTED
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

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
// Global Windows host callback, needed while plugin is being created

WinHostCallback sWinHostCallback = NULL;

// --------------------------------------------------------------------------------------------------------------------
// Combined Windows + Linux effect with extra data for custom wrapping

typedef struct {
    WinEffect winfx;
    void* linuxlib;
    LinuxEffect* linuxfx;
    LinuxWrappedVSTPluginMainFn linuxwrapper;
    WinHostCallback winHostCallback;
   #ifdef WINESULIN_GUI_IS_SUPPORTED
    struct {
        Display* display;
        Window window;
        unsigned width, height;
        bool visible;
    } x11;
    HWND hwnd, parent, fullparent;
   #endif
} CombinedEffect;

// --------------------------------------------------------------------------------------------------------------------
// Linux wrapped plugin host callback
// converts plugin calls meant for the host from Linux to Windows ones

static intptr_t linuxHostCallback(LinuxEffect* linuxfx, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    if (linuxfx == NULL)
        return sWinHostCallback != NULL ? sWinHostCallback(NULL, opcode, index, value, ptr, opt) : 0;

    CombinedEffect* const effect = linuxfx->hostPtr1;

    switch (opcode)
    {
    // size window
    case 15:
       #ifdef WINESULIN_GUI_IS_SUPPORTED
        if (effect->x11.display != NULL && effect->x11.window != 0)
        {
            effect->x11.width = index;
            effect->x11.height = value;
            XResizeWindow(effect->x11.display, effect->x11.window, index, value);

            for (XEvent event; XPending(effect->x11.display) != 0;)
                XNextEvent(effect->x11.display, &event);
        }
        break;
       #endif
    }

    return effect->winHostCallback(effect, opcode, index, value, ptr, opt);
}

#ifdef WINESULIN_GUI_IS_SUPPORTED

// --------------------------------------------------------------------------------------------------------------------
// Window move event hook

static void CALLBACK winMoveEventHook(HWINEVENTHOOK hook,
                                      DWORD event,
                                      HWND hwnd,
                                      LONG idObject,
                                      LONG idChild,
                                      DWORD idEventThread,
                                      DWORD time)
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    if (event != EVENT_OBJECT_LOCATIONCHANGE)
        return;
    if (idObject != OBJID_WINDOW)
        return;
    if (idChild != CHILDID_SELF)
        return;

    CombinedEffect* effect = (CombinedEffect*)GetPropA(hwnd, WINESULIN_EFFECT_PROP_NAME);
    if (effect == NULL)
        return;

    RECT rect;
    RECT offset = { 0 };
    if (GetWindowRect(effect->hwnd, &rect))
    {
        if (effect->fullparent != NULL && effect->fullparent != effect->hwnd)
            GetWindowRect(effect->fullparent, &offset);

       #ifdef WINESULIN_GUI_IS_EMBED
        const int x = rect.left - offset.left;
        const int y = rect.top - offset.top;
       #else
        const int x = rect.left;
        const int y = rect.top;
       #endif

        XMoveWindow(effect->x11.display, effect->x11.window, x, y);

        for (XEvent event; XPending(effect->x11.display) != 0;)
            XNextEvent(effect->x11.display, &event);
    }

    fprintf(stderr, "%s END\n", __FUNCTION__);
}

// --------------------------------------------------------------------------------------------------------------------
// Window class management

static void registerWindowClass()
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    WNDCLASSEXA windowClass = { 0 };
    if (GetClassInfoExA(sInstance, WINESULIN_CLASS_NAME, &windowClass))
        return; // Already registered

    windowClass.cbSize        = sizeof(windowClass);
    windowClass.style         = CS_OWNDC;
    windowClass.lpfnWndProc   = DefWindowProc;
    windowClass.hInstance     = sInstance;
    windowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    // windowClass.hbrBackground = GetStockObject(BLACK_BRUSH); // (HBRUSH)
    windowClass.lpszClassName = WINESULIN_CLASS_NAME;
    RegisterClassExA(&windowClass);
}

static void unregisterWindowClass()
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    UnregisterClassA(WINESULIN_CLASS_NAME, NULL);
}

// --------------------------------------------------------------------------------------------------------------------

static intptr_t createWindow(CombinedEffect* effect, HWND parent)
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    const int16_t width = effect->x11.width ?: 1;
    const int16_t height = effect->x11.height ?: 1;

    if (effect->hwnd == NULL)
    {
        registerWindowClass();

        effect->hwnd = CreateWindowExA(WS_EX_NOINHERITLAYOUT,
                                       WINESULIN_CLASS_NAME,
                                       WINESULIN_CLASS_NAME,
                                       WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                       0,
                                       0,
                                       width,
                                       height,
                                       parent,
                                       NULL,
                                       sInstance,
                                       NULL);

        if (effect->hwnd == NULL)
        {
            fprintf(stderr, "[winesulin] Failed to create win32 window\n");
            return 0;
        }
    }

    HWND fullparent = parent;
    while ((GetWindowLongA(fullparent, GWL_STYLE) & WS_CHILD) != 0)
    {
        fullparent = GetParent(fullparent);
    }

    effect->parent = parent;
    effect->fullparent = fullparent;

    const Window fullwindow = (Window)GetPropA(fullparent, "__wine_x11_whole_window");
    fprintf(stderr, "[winesulin] DEBUG WINDOW %p -> %p -> %lu\n", parent, fullparent, fullwindow);

    if (effect->x11.display == NULL)
    {
        effect->x11.display = XOpenDisplay(NULL);

        if (effect->x11.display == NULL)
        {
            fprintf(stderr, "[winesulin] Failed to open X11 display\n");
            return 0;
        }
    }

    if (effect->x11.window == 0)
    {
        const int screen = DefaultScreen(effect->x11.display);

        XSetWindowAttributes attr = { 0 };
        attr.border_pixel = 0;
        attr.background_pixel = 0;

        effect->x11.window = XCreateWindow(effect->x11.display,
                                          #ifdef WINESULIN_GUI_IS_EMBED
                                           fullwindow ?:
                                          #endif
                                           RootWindow(effect->x11.display, screen),
                                           0, 0, width, height, 0,
                                           DefaultDepth(effect->x11.display, screen),
                                           InputOutput,
                                           DefaultVisual(effect->x11.display, screen),
                                           CWBorderPixel | CWBackPixel,
                                           &attr);

        if (effect->x11.window == 0)
        {
            fprintf(stderr, "[winesulin] Failed to create X11 window\n");
            return 0;
        }
    }

    ShowWindow(effect->hwnd, SW_HIDE);
    // ShowWindow(effect->hwnd, SW_SHOW);
    SetPropA(effect->fullparent, WINESULIN_EFFECT_PROP_NAME, effect);

    DWORD processId;
    DWORD threadId = GetWindowThreadProcessId(effect->hwnd, &processId);
    SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,
                    EVENT_OBJECT_LOCATIONCHANGE,
                    NULL,
                    winMoveEventHook,
                    processId,
                    threadId,
                    WINEVENT_OUTOFCONTEXT);

    XStoreName(effect->x11.display, effect->x11.window, WINESULIN_CLASS_NAME);

    {
        Atom wmDelete = XInternAtom(effect->x11.display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(effect->x11.display, effect->x11.window, &wmDelete, 1);
    }

   #ifdef WINESULIN_GUI_IS_EMBED
    if (fullwindow == 0)
   #endif
    {
        const Atom windowType = XInternAtom(effect->x11.display, "_NET_WM_WINDOW_TYPE", False);
        const Atom windowTypes[] = {
            XInternAtom(effect->x11.display, "_NET_WM_WINDOW_TYPE_NORMAL", False),
            XInternAtom(effect->x11.display, "_NET_WM_WINDOW_TYPE_UTILITY", False),
        };
        XChangeProperty(effect->x11.display,
                        effect->x11.window,
                        windowType,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (const unsigned char*)windowTypes,
                        sizeof(windowTypes)/sizeof(windowTypes[0]));

        const unsigned long wmHints[] = {
            2, 0, 0, 0, 0,
        };
        const Atom motifWmHints = XInternAtom(effect->x11.display, "_MOTIF_WM_HINTS", False);
        XChangeProperty(effect->x11.display,
                        effect->x11.window,
                        motifWmHints,
                        motifWmHints,
                        32,
                        PropModeReplace,
                        (const unsigned char *)wmHints,
                        sizeof(wmHints)/sizeof(wmHints[0]));

        if (fullwindow != 0)
            XSetTransientForHint(effect->x11.display, effect->x11.window, fullwindow);
    }

    XFlush(effect->x11.display);

    return effect->linuxfx->dispatcher(effect->linuxfx, 14, 0, 0, (void*)effect->x11.window, 0.f);
}

// --------------------------------------------------------------------------------------------------------------------

static intptr_t destroyWindow(CombinedEffect* effect)
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    if (effect->x11.display == NULL || effect->x11.window == 0 || effect->hwnd == NULL)
        return 0;

    ShowWindow(effect->hwnd, SW_HIDE);
    XUnmapWindow(effect->x11.display, effect->x11.window);

    const intptr_t ret = effect->linuxfx->dispatcher(effect->linuxfx, 15, 0, 0, 0, 0.f);

    DestroyWindow(effect->hwnd);
    effect->hwnd = effect->fullparent = NULL;
    unregisterWindowClass();

    XDestroyWindow(effect->x11.display, effect->x11.window);
    effect->x11.window = 0;
    effect->x11.visible = false;

    XCloseDisplay(effect->x11.display);
    effect->x11.display = NULL;

    return ret;
}

// --------------------------------------------------------------------------------------------------------------------

static intptr_t idleWindow(CombinedEffect* effect)
{
    // fprintf(stderr, "%s\n", __FUNCTION__);

    if (effect->x11.display == NULL || effect->x11.window == 0 || effect->hwnd == NULL || effect->parent == NULL)
    {
        fprintf(stderr, "[winesulin] Idle with bad setup!\n");
        return 0;
    }

    {
        int x, y;
        RECT rect;
        RECT offset = { 0 };
        if (GetWindowRect(effect->hwnd, &rect))
        {
            if (effect->fullparent != NULL && effect->fullparent != effect->hwnd)
                GetWindowRect(effect->fullparent, &offset);

           #ifdef WINESULIN_GUI_IS_EMBED
            x = rect.left - offset.left;
            y = rect.top - offset.top;
           #else
            x = rect.left;
            y = rect.top;
           #endif
        }
        else
        {
            x = y = 0;
        }

        XMoveWindow(effect->x11.display, effect->x11.window, x, y);
        // XMoveResizeWindow(effect->x11.display, effect->x11.window, x, y, effect->x11.width, effect->x11.height);
    }

    if (IsWindowVisible(effect->parent))
    {
        if (! effect->x11.visible)
        {
            XMapRaised(effect->x11.display, effect->x11.window);
            effect->x11.visible = true;
        }
    }
    else
    {
        if (effect->x11.visible)
        {
            XUnmapWindow(effect->x11.display, effect->x11.window);
            effect->x11.visible = false;
        }
    }

    for (XEvent event; XPending(effect->x11.display) != 0;)
        XNextEvent(effect->x11.display, &event);

    return effect->linuxfx->dispatcher(effect->linuxfx, 19, 0, 0, 0, 0.f);
}

#endif // WINESULIN_GUI_IS_SUPPORTED

// --------------------------------------------------------------------------------------------------------------------

static intptr_t __cdecl win_dispatcher(CombinedEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
    if (effect == NULL)
        return 0;

//     if (opcode == 19 || opcode == 25)
//         return 0;

//     if (opcode != 19 && opcode != 25)
        fprintf(stderr, "%s %d %d %ld %p %f\n", __FUNCTION__, opcode, index, value, ptr, opt);

    intptr_t ret;

    switch (opcode)
    {
    // close
    case 1:
       #ifdef WINESULIN_GUI_IS_SUPPORTED
        destroyWindow(effect);
       #endif
        ret = effect->linuxfx->dispatcher(effect->linuxfx, opcode, index, value, ptr, opt);
        free(effect);
        return ret;

   #ifdef WINESULIN_GUI_IS_SUPPORTED
    case 13:
        if (ptr != NULL)
        {
            ret = effect->linuxfx->dispatcher(effect->linuxfx, opcode, index, value, ptr, opt);
            typedef int16_t rect4[4];
            rect4* const r = *(rect4**)ptr;
            effect->x11.width = (*r)[3] - (*r)[1];
            effect->x11.height = (*r)[2] - (*r)[0];
            return ret;
        }
        return 0;

    case 14:
        return createWindow(effect, ptr);

    case 15:
        return destroyWindow(effect);

    case 19:
        return idleWindow(effect);
   #else
    case 13:
    case 14:
    case 15:
    case 19:
        return 0;
   #endif
    }

    return effect->linuxfx->dispatcher(effect->linuxfx, opcode, index, value, ptr, opt);
}

// --------------------------------------------------------------------------------------------------------------------

static float __cdecl win_getParameter(CombinedEffect* effect, int32_t index)
{
    fprintf(stderr, "%s\n", __FUNCTION__);
    return effect->linuxfx->getParameter(effect->linuxfx, index);
}

static void __cdecl win_setParameter(CombinedEffect* effect, int32_t index, float value)
{
    fprintf(stderr, "%s\n", __FUNCTION__);
    effect->linuxfx->setParameter(effect->linuxfx, index, value);
}

static void __cdecl win_process(CombinedEffect* effect, float** inputs, float** outputs, int32_t sampleFrames)
{
    fprintf(stderr, "%s\n", __FUNCTION__);
    effect->linuxfx->process(effect->linuxfx, inputs, outputs, sampleFrames);
}

static void __cdecl win_processReplacing(CombinedEffect* effect, float** inputs, float** outputs, int32_t sampleFrames)
{
    // fprintf(stderr, "%s\n", __FUNCTION__);
    effect->linuxfx->processReplacing(effect->linuxfx, inputs, outputs, sampleFrames);
}

static void __cdecl win_processDoubleReplacing(CombinedEffect* effect, double** inputs, double** outputs, int32_t sampleFrames)
{
    // fprintf(stderr, "%s\n", __FUNCTION__);
    effect->linuxfx->processDoubleReplacing(effect->linuxfx, inputs, outputs, sampleFrames);
}

// --------------------------------------------------------------------------------------------------------------------

__attribute__ ((visibility("default")))
void* __cdecl VSTPluginMain(WinHostCallback winHostCallback)
{
    fprintf(stderr, "%s %d | %p\n", __FUNCTION__, __LINE__, winHostCallback);

    // safety checks first
    if (winHostCallback == NULL || winHostCallback(NULL, 1, 0, 0, NULL, 0.f) == 0)
        return NULL;

    // init only once
    static bool init = true;
    static void* linuxlib = NULL;
    static LinuxWrappedVSTPluginMainFn linuxwrapper = NULL;

    if (init)
    {
        init = false;

        // get current dll filename
        WCHAR wfilename[MAX_PATH];
        const DWORD len = GetModuleFileNameW(sInstance, wfilename, MAX_PATH);

        // ensure valid path ending with ".dll"
        if (len < 5 || len >= MAX_PATH - 5)
            return NULL;
        // TODO use strcmp similar
        if (wfilename[len-4] != '.' || wfilename[len-3] != 'd' || wfilename[len-2] != 'l' || wfilename[len-1] != 'l')
            return NULL;

        // replace filename extension ".dll" -> ".so"
        wfilename[len - 3] = 's';
        wfilename[len - 2] = 'o';
        wfilename[len - 1] = 0;

        // convert to unix filename
        char* const filename = wine_get_unix_file_name(wfilename);
        if (filename == NULL)
        {
            fprintf(stderr, "[winesulin] Failed to convert '%S' into a unix path\n", wfilename);
            return NULL;
        }

        // HeapFree(sInstance, 3, filename);

        // load linux wrapped plugin
        linuxlib = dlopen(filename, RTLD_NOW|RTLD_LOCAL);

        if (linuxlib == NULL)
        {
            fprintf(stderr, "[winesulin] Failed to open Linux plugin: %s\n", dlerror());
            return NULL;
        }

        linuxwrapper = dlsym(linuxlib, "VSTPluginMain") ?: dlsym(linuxlib, "main");

        if (linuxwrapper == NULL)
        {
            fprintf(stderr, "[winesulin] Failed to find plugin entry point: %s\n", dlerror());
            dlclose(linuxlib);
            linuxlib = NULL;
            return NULL;
        }

        fprintf(stderr, "[winesulin] Sucessfully loaded Linux wrapped plugin '%s'\n", filename, linuxwrapper);
    }

    // safety checks, we might have failed to load the linux plugin
    if (linuxlib == NULL || linuxwrapper == NULL)
        return NULL;

    // call VSTPluginMain from plugin side to create the linux effect
    sWinHostCallback = winHostCallback;
    LinuxEffect* const linuxfx = linuxwrapper(linuxHostCallback);
    sWinHostCallback = NULL;

    if (linuxfx == NULL)
    {
        fprintf(stderr, "[winesulin] Error: Linux wrapped plugin returned NULL\n");
        dlclose(linuxlib);
        linuxlib = NULL;
        return NULL;
    }

    // allocate and store all useful pointers
    CombinedEffect* effect = calloc(1, sizeof(CombinedEffect));
    effect->linuxlib = linuxlib;
    effect->linuxfx = linuxfx;
    effect->linuxwrapper = linuxwrapper;
    effect->winHostCallback = winHostCallback;

    // copy Linux wrapped effect data to hosted Windows side
    memcpy(&effect->winfx, linuxfx, sizeof(WinEffect));

    // replace Windows calls with our custom conversions
    effect->winfx.dispatcher       = win_dispatcher;
    effect->winfx.getParameter     = win_getParameter;
    effect->winfx.setParameter     = win_setParameter;
    effect->winfx.process          = win_process;
    effect->winfx.processReplacing = win_processReplacing;
    effect->winfx.processDoubleReplacing = win_processDoubleReplacing;

    // use "host" pointer on linux side to store our custom effect instance
    linuxfx->hostPtr1 = effect;

   #ifndef WINESULIN_GUI_IS_SUPPORTED
    // no editor
    effect->winfx.flags &= ~1;
   #endif

    return effect;
}
