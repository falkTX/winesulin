/*
 */

#pragma once

#ifndef _WIN32
#error _WIN32 is undefined
#endif
#ifndef __cdecl
#error __cdecl is undefined
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// --------------------------------------------------------------------------------------------------------------------
// Linux wrapped host callback, to be triggered from the Linux plugin side, needs conversion to Windows

typedef intptr_t (*LinuxWrappedHostCallback)(void*, int32_t, int32_t, intptr_t, void*, float);

// --------------------------------------------------------------------------------------------------------------------
// type used for Linux wrapped VSTPluginMain calls
// loaded from the Linux plugin to be wrapped

typedef void* (*LinuxWrappedVSTPluginMainFn)(LinuxWrappedHostCallback);

// --------------------------------------------------------------------------------------------------------------------
// Windows host provided callback, to be triggered from the Linux plugin side after conversion

typedef intptr_t (__cdecl *WinHostCallback)(void*, int32_t, int32_t, intptr_t, void*, float);

// --------------------------------------------------------------------------------------------------------------------
// type used for Windows VSTPluginMain calls
// we wrap a real win32 binary (called from the host) into a winelib that exposes the same function
// doing it this way allows hosts to correctly identify win32 dlls, that would otherwise see a mixed win/linux dll

typedef void* (__cdecl *WinVSTPluginMainFn)(WinHostCallback);

// --------------------------------------------------------------------------------------------------------------------
// Linux and Windows effect data structures

#define EFFECT_RESERVED_SIZE (11 * sizeof(int32_t) + 4 * sizeof(void*))
#define EFFECT_PADDING 64

#pragma pack(push, 8)

typedef struct {
    int32_t magic;
    intptr_t (*dispatcher)(void*, int32_t, int32_t, intptr_t, void*, float);
    void (*process)(void*, float**, float**, int32_t);
    void (*setParameter)(void*, int32_t, float);
    float (*getParameter)(void*, int32_t);
    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;
    int32_t flags;
    void* hostPtr1;
    void* hostPtr2;
    int32_t delay;
    int32_t unused[2];
    float unusedf;
    void* effectPtr;
    void* userPtr;
    int32_t unique_id;
    int32_t version;
    void (*processReplacing)(void*, float**, float**, int32_t);
    uint8_t padding[EFFECT_PADDING];
} LinuxEffect;

typedef struct {
    int32_t magic;
    intptr_t (__cdecl *dispatcher)(void*, int32_t, int32_t, intptr_t, void*, float);
    void (__cdecl *process)(void*, float**, float**, int32_t);
    void (__cdecl *setParameter)(void*, int32_t, float);
    float (__cdecl *getParameter)(void*, int32_t);
    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;
    int32_t flags;
    void* hostPtr1;
    void* hostPtr2;
    int32_t delay;
    int32_t unused[2];
    float unusedf;
    void* effectPtr;
    void* userPtr;
    int32_t unique_id;
    int32_t version;
    void (__cdecl *processReplacing)(void*, float**, float**, int32_t);
    uint8_t padding[EFFECT_PADDING];
} WinEffect;

#pragma pack(pop)

_Static_assert(sizeof(LinuxEffect) == sizeof(WinEffect), "mismatch effect struct size");

// --------------------------------------------------------------------------------------------------------------------
