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

## 0.3.3-335 — selected client pin
TEST installation and VERIFY / REPAIR reject a game package if its Wow.exe SHA256 is not 2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d. Updater builds independently of the uploaded EXE; a successful updater build is not a playable game package when there are no registered game DLLs.

## 0.3.4-335 — niezależny test AutoLoot i wysyłka zdarzeń z gry

W eksperymentalnej gałęzi `feature/autoloot-12340` updater Windows x86
zawiera pliki dodatku `WoW335AutoLootDiag` jako zasoby kompilowane z
dokładnego SHA updatera. Przycisk **Instaluj test AutoLoot** instaluje go
do wybranego katalogu `Interface/AddOns`, po sprawdzeniu dokładnej
binarki `Wow.exe`. Jeśli folder dodatku już istnieje, użytkownik
potwierdza podmianę, a updater przenosi CAŁY poprzedni folder do
`.wow335_updater/autoloot_diag_backups/` i zachowuje możliwość
odzyskania; zgodna istniejąca kopia zostaje jedynie oznaczona jako
zarządzana. Nie modyfikuje innych addonów, gry, DLL, `dlls.txt` ani
nie udaje paczki `FINAL_PACKAGE: PASS`.

W grze: `/al335 on`, otwórz zwłoki **ręcznie**, użyj `/reload`
lub wyloguj się i zamknij grę. Przycisk **Wyślij log AutoLoot** lokalizuje
`WTF/Account/*/SavedVariables/WoW335AutoLootDiag.lua` (alternatywnie
`WoW335AutoLootDiagLog.lua`), wyodrębnia tylko ograniczoną liczbę
dozwolonych linii zdarzeń i pokazuje **podgląd przed wysłaniem**
jako GitHub Issue do `github12wykrzyk/335`. Nie przesyła surowych
SavedVariables, nazw katalogów kont ani innych plików. Osobny token
Issues: Read and write pozostaje zapisany przez DPAPI; użytkownik
sam potwierdza wysyłkę. Ogólne **Wyślij raport** dołącza też te
ograniczone linie zdarzeń (jeśli istnieją) do dotychczasowej diagnostyki.

To dodatek diagnostyczny **manual-loot-window only**, nie natywny AutoLoot.
Updater na kanale TEST (work) nadal odrzuca game-package, dopóki
nie ma aktywnej zweryfikowanej DLL oraz `FINAL_PACKAGE: PASS`.
Nowy eksperymentalny updater pobiera się osobno z udanego workflow
`Build 335 updater` na dokładnym SHA gałęzi feature.
