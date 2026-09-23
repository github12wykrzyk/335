#ifndef WOW335_AUTOLOOT_12340_ADAPTER_H
#define WOW335_AUTOLOOT_12340_ADAPTER_H
#include "autoloot_core.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AL12340_EXACT_EXE_SHA256 "2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d"
#define AL12340_CLIENT_CONNECTION_VA ((uintptr_t)0x00C79CE0u)
#define AL12340_MANAGER_OFFSET ((uintptr_t)0x2ED0u)
#define AL12340_RIGHT_CLICK_VA ((uintptr_t)0x00731260u)
#define AL12340_LUA_EXECUTE_VA ((uintptr_t)0x00819210u)
#define AL12340_LOOTABLE_FIELD 0x4Fu
#define AL12340_LOOTABLE_BIT 0x1u

/* Host methods must run in the validated WoW game thread. This adapter
 * neither injects itself nor installs hooks. Callers may not mark a tentative
 * ABI as verified without examining the exact pinned client. */
typedef struct {
    void *context;
    int (*verify_exe_sha256)(void *, const char *);
    int (*verify_abi)(void *, uintptr_t, uintptr_t);
    uint32_t (*thread_id)(void *);
    int (*read_u32)(void *, uintptr_t, uint32_t *);
    int (*position)(void *, uintptr_t, float [3]);
    int (*right_click)(void *, uintptr_t, uintptr_t, unsigned);
    AlUiState (*owned_loot_window)(void *, AlGuid);
    int (*execute_lua)(void *, uintptr_t, const char *, const char *);
    int (*can_act)(void *);
} Al12340Host;
typedef struct {
    AlEngine engine;
    Al12340Host host;
    uint32_t game_thread;
    unsigned bound;
} Al12340Adapter;

#if defined(_WIN32)
#define AL12340_API __declspec(dllexport)
#else
#define AL12340_API
#endif

/* An unverified caller cannot bind; OFF by default. */
AL12340_API int al12340_bind(Al12340Adapter *, const Al12340Host *, float range_yards);
AL12340_API void al12340_enable(Al12340Adapter *, int);
AL12340_API void al12340_tick(Al12340Adapter *, uint32_t now_ms);
#ifdef __cplusplus
}
#endif
#endif
