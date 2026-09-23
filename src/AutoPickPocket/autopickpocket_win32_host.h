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
    int (*spell_usable)(void *,uint32_t spell_id);
    PpResult (*cast_result)(void *,PpGuid target);
    uint64_t (*world_token)(void *);
} Pp335Policy;
PP335_EXPORT int __stdcall PP335_BindOnGameThread(const Pp335Policy *policy);
PP335_EXPORT void __stdcall PP335_EnableOnGameThread(int enable);
PP335_EXPORT void __stdcall PP335_TickOnGameThread(uint32_t now_ms);
PP335_EXPORT void __stdcall PP335_ResetOnGameThread(void);
#ifdef __cplusplus
}
#endif
#endif
