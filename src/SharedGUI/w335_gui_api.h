/* Shared in-process GUI ABI v1. C/stdcall, Windows PE32 x86, game thread only. */
#ifndef W335_GUI_API_H
#define W335_GUI_API_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define W335GUI_ABI_VERSION 1u
#define W335GUI_MAX_FIELDS 16u
#define W335GUI_TOGGLE 1u
#define W335GUI_RANGE 2u
typedef struct {
    const char *key;       /* stable ASCII ini key, no path separators */
    const char *label;     /* UI caption */
    unsigned kind;         /* TOGGLE or RANGE */
    int minimum, maximum, initial;
} W335GUI_Field;
typedef void (WINAPI *W335GUI_OnChange)(const char *key,int value);
typedef struct {
    unsigned size;        /* sizeof(W335GUI_Module) */
    unsigned abi_version; /* W335GUI_ABI_VERSION */
    const char *id;       /* stable module ID */
    const char *filename; /* actual mapped DLL basename */
    const char *title;
    unsigned field_count;
    const W335GUI_Field *fields;
    W335GUI_OnChange on_change; /* called on game/window thread only */
} W335GUI_Module;
typedef int (WINAPI *W335GUI_RegisterFn)(const W335GUI_Module *module);
typedef void (WINAPI *W335GUI_UnregisterFn)(const char *id);
#endif
