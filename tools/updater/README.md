# WoW335Updater 0.1.0-335 — experimental port
Windows x86 WinForms external updater, bound ONLY to github12wykrzyk/335. Does not inject DLLs.
TEST(work): newest successful Build 335 work candidate; STABLE(main): newest successful Build 335 stable candidate. Requires actual final-gated runtime artifacts; empty bootstrap has none.
Validates package SHA256 from candidate_metadata.json before installing, keeps managed-file backups, supports rollback, verify/repair, update+play, updater self-update, optional sanitized issue reports. Current updater requires a fine-grained GitHub token with Contents: Read and Actions: Read for this repository (including artifact downloads). A separate optional report token grants Issues: Read and write only. Never commit tokens.
The 1.12 realmlist selector is DISABLED pending localization-aware WoW 3.3.5a path support. Do not alter existing Data/<locale>/realmlist.wtf.
The compiled updater is not yet proof of correct game integration or tested in-game; require exact 12340 in-game test before stable promotion.

## 0.3.0-335 — ice/teal desktop skin
The single-screen updater retains the work/main GitHub status monitor, now shown
as slim outlined text strips in the header (10 s refresh). The standalone
`tools/updater/assets/WoW335_ice.ico` is embedded as the native application icon
of both updater and bootstrap Windows x86 executables; the main window explicitly
uses its executable icon. The badges display CI state only, not proof of a
verified runnable WoW game package.

## 0.3.2-335 — exact HEAD/package identity and managed-file protection
TEST/STABLE game artifacts must belong to current branch HEAD, not a stale successful run. The inner package metadata must match exact run SHA, channel, game build and x86. Existing unmanaged DLL/dlls.txt filename collisions are blocked before writes; the explicitly selected target EXE is backed up. Full-package rollback remains supported. No active 12340 game DLL has been certified by this updater build.
