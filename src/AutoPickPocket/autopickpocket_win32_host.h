#ifndef WOW335_PP_NATIVE_HOST_H
#define WOW335_PP_NATIVE_HOST_H
#include "autopickpocket_12340_adapter.h"
#ifdef _WIN32
#include <windows.h>
#define PP335_EXPORT __declspec(dllexport)
#else
#define PP335_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Only trusted shared game-thread loader may supply these policies. They must
 * outlive the module and return 0/PP_RESULT_PENDING on uncertainty.
 * This module owns neither movement nor AutoLoot, and installs no hooks.
 */
typedef struct {
    void *context;
    int (*eligible_npc)(void *,uintptr_t obj,PpGuid target);
    /* 3.3.5 CreatureType.dbc ID: 6=Undead, 7=Humanoid; 0=unknown. */
    uint32_t (*creature_type)(void *,uintptr_t obj,PpGuid target);
    int (*spell_usable)(void *,uint32_t spell_id);
    /* Arms authoritative correlation before each native cast. Return 0 if
     * the observer cannot associate a result with this GUID and nonce. */
    int (*begin_attempt)(void *,PpGuid target,uint32_t attempt_id);
    PpResult (*cast_result)(void *,PpGuid target,uint32_t attempt_id);
    uint64_t (*world_token)(void *);
} Pp335Policy;
PP335_EXPORT int __stdcall PP335_BindOnGameThread(const Pp335Policy *policy);
PP335_EXPORT void __stdcall PP335_EnableOnGameThread(int enable);
PP335_EXPORT void __stdcall PP335_TickOnGameThread(uint32_t now_ms);
PP335_EXPORT void __stdcall PP335_ResetOnGameThread(void);
/* Native dispatcher for the authorized in-game /appp slash registration. */
PP335_EXPORT int __stdcall PP335_CommandOnGameThread(const char *arguments);
/* Work updater's manifest-driven launcher hook ABI. This is a separate hook
 * chain, never a second installer of AutoLoot's game interaction hooks. */
PP335_EXPORT UINT WINAPI W335_MessageId(void);
PP335_EXPORT LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l);
PP335_EXPORT LRESULT CALLBACK W335_CallWndProc(int code, WPARAM w, LPARAM l);
#ifdef __cplusplus
}
#endif
#endif
