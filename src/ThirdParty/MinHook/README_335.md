MinHook (x86) vendored source from https://github.com/TsudaKageyu/minhook
Pinned commit: 8af6b4acae5a9388fd742b56fa79ece89d96f823
Files: LICENSE.txt, include/MinHook.h, src/buffer.c, src/buffer.h, src/hde/hde32.c, src/hde/hde32.h, src/hde/pstdint.h, src/hde/table32.h, src/hook.c, src/trampoline.c, src/trampoline.h
License: LICENSE.txt (2-clause BSD). This code is linked exclusively inside PlayerESP335.dll.
No source code or binaries copied from WoW112.

Local x86 build-only adaptation: per-source MSVC warning suppressions for upstream C4201/C4100/C4310/C4244 in buffer.c/hook.c/trampoline.c; /W4 /WX remains enabled for PlayerESP-owned sources. Vendored core algorithm and license otherwise unchanged.
