# 12340 shared game-thread loader — source integration candidate

Branch: feature/autopickpocket-12340. This is NOT a playable TEST package.

The startup DLL is based on the existing feature/loader-12340 experiment and
must only be activated after the exact EpochConnection import extension and the
updater's modules.lock have been verified on the selected Wow.exe. It reads
dlls.txt and validates every entry and its SHA256 before loading anything.

When both AutoLoot335.dll and AutoPickPocket335.dll are listed, the loader
resolves the PP bind/tick/enable exports and requires a concrete
PP335_VerifiedPolicyV1 provider. It checks all five policy callbacks, installs
the two EXISTING AutoLoot message hooks once, asks AutoLoot to bind PP on the
window thread and checks AL335_PeerStatus()==2 before accepting the startup.
No second PP hook exists; no game API is invoked from the worker. A failed
policy or game-thread bind blocks the combined stack instead of falling back
silently to AutoLoot-only mode. The module starts PP disabled; result observation
must still be implemented before any real Pick Pocket attempt is enabled.

The companion AutoLoot game host adds an explicit peer configuration interface.
In a combined runtime, it calls PP bind and tick on its verified window thread,
then disables PP on shutdown. The old standalone AutoLoot hook exports remain
present; this source is not a byte-identical replacement for the registered
AutoLoot runtime DLL. The dedicated build_shared_loader.yml compiles all three
real Windows PE32 x86 DLLs and publishes SHA256 build evidence marked
UNREGISTERED. Do not install those artifacts manually or mix them with the
currently registered AutoLoot DLL.

Remaining delivery gates: native PP NPC/usable/selected-GUID/result policies,
exact PP+AutoLoot+loader binary/manifest registration, updater's transactional
multi-DLL installation, import-entrypoint verification, strict build_active
source/binary equality, FINAL_PACKAGE: PASS for the same SHA and user game test.
