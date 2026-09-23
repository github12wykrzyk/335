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

Na kanale `work` updater Windows x86
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
Nie pobieraj oddzielnych paczek dodatku. Na istniejącym updaterze
v0.3.3-335 wybierz **Aktualizuj updater** z kanału TEST (work), aby
pobrać z GitHub Actions dokładnie zweryfikowany updater najnowszego
SHA gałęzi `work`. Po restarcie v0.3.4-335 użyj przycisków
**Instaluj test AutoLoot** i **Wyślij log AutoLoot**. Nie jest
wymagana ręczna instalacja ZIP. Instalacja dodatku i raportowanie
są niezależne od braku natywnej paczki gry; zwykła aktualizacja
gry i VERIFY/REPAIR nadal wymagają FINAL_PACKAGE: PASS.

Zabezpieczenia self-update v0.3.4: najnowszy wynik workflow musi być
udany i odpowiadać aktualnemu HEAD wybranego kanału; wymagane są
zgodne SHA artefaktu i `updater_build.json` (git_sha, channel,
SHA256 updatera i bootstrapa). Brak gotowego workflow nie uruchamia
instalacji starszej wersji updatera.

## 0.3.5-335 — odnajdywanie SavedVariables AutoLoot

Przycisk **Wyślij log AutoLoot** najpierw skanuje lokalizację
`<KATALOG_GRY>/WTF/Account/<konto>/SavedVariables/WoW335AutoLootDiag.lua`
oraz pliki `.lua.bak`; parser czyta zarówno tablice Lua z kolejnymi
wierszami tekstu, jak i indeksowane `[1] = "zdarzenie"` /
`["1"] = "zdarzenie"`. Jeśli w wybranej instalacji gry nie ma
rozpoznanych zdarzeń, updater pokazuje lokalną diagnozę (czy folder
dodatku i WTF istnieją; nazwy i daty znalezionych plików) i oferuje
wskazanie konkretnego zapisanego pliku. Wybór odbywa się w updaterze,
bez kopiowania/edycji pliku; upload wysyła jedynie dozwolone,
sanityzowane linie zdarzeń, po podglądzie i zatwierdzeniu. Lokalna
ścieżka do konta NIE jest logowana do ogólnego raportu GitHub.

Aby WoW utworzył SavedVariables, **uruchom zainstalowany dodatek**,
sprawdź w czacie `/al335 status`, włącz go przez `/al335 on`,
otwórz ręcznie zwłoki i użyj `/reload` lub wyloguj się. Jeśli
grę uruchamiasz z innej instalacji niż wybrany katalog updatera,
zapis będzie w katalogu `WTF` tamtej instalacji.

## 0.3.6-335 — obsługa rzeczywistych logów z WoW

Parser wpisów SavedVariables akceptuje końcowe komentarze Lua `-- [1]`
po każdej pozycji tablicy, które standardowy klient dopisuje podczas
zapisywania `WoW335AutoLootDiag.lua`. Poprzednia wersja 0.3.5
odrzucała wszystkie takie wiersze pomimo poprawnego pliku.
Ręczne wskazanie kopii `WoW335AutoLootDiag(1).lua` także jest obsługiwane.
Aktualizacja wyłącznie przyciskiem **Aktualizuj updater** z kanału TEST
(work); do GitHub wysyłane są jedynie zaakceptowane zdarzenia po podglądzie.

## 0.3.7-335 — natywny AutoLoot tylko jako świadomie uruchamiany eksperyment

Przycisk **Natywny AutoLoot TEST** jest oddzielony od zwykłego
**Aktualizuj** i `FINAL_PACKAGE`. Jego jedyne źródło to najnowszy
zakończony sukcesem workflow `Build isolated 12340 AutoLoot native host`
na aktualnym SHA `feature/autoloot-12340`; brak bieżącego artefaktu,
mismatched SHA, zła architektura lub hash EXE przerywają działanie.
Updater sprawdza metadane `NATIVE_AUTOLOOT_PREVIEW_NOT_GAME_PACKAGE`,
SHA256 obu plików, dokładną wersję PE32 x86 i świadome potwierdzenie
przed uruchomieniem. Pliki trafiają wyłącznie pod
`.wow335_updater/native_preview/<SHA>`; nie nadpisują Wow.exe,
innych DLL, dlls.txt, innych dodatków ani stable, a istniejące
niezgodne pliki zostają zachowane i użycie jest blokowane.

Wybierz dokładny katalog gry, zamknij działającego klienta, wybierz
kanał TEST, kliknij **Natywny AutoLoot TEST** i potwierdź ostrzeżenie.
Natywny launcher uruchamia wybrany Wow.exe i próbuje podłączyć
WH_GETMESSAGE tylko do jego głównego wątku okna. Działa jedynie dla
SHA256 klienta przypiętego w repo; może nie działać, jeśli OS lub
konfiguracja gry odrzuci hook bądź wymagany wątek Lua będzie inny.
Natywny eksperyment nie jest jeszcze zaakceptowany jako aktywna
paczka DLL. Nie uruchamiaj go podczas zwykłego lootu z innym
modułem ingerującym w interakcje. W razie problemów zamknij grę;
sam launcher kończy się i odłącza hook po zamknięciu gry.

## 0.3.8-335 — zarejestrowany AutoLoot335.dll w TEST/work

AutoLoot nie jest już pobierany jako osobny natywny podgląd, gdy
`runtime/current.json` i pełna paczka z dokładnego SHA zawierają
zarejestrowany moduł. **Aktualizuj** instaluje tylko kompletny, zweryfikowany
zestaw `Wow.exe + AutoLoot335.dll + dlls.txt` z udanego workflow
`FINAL_PACKAGE: PASS`. Przy instalacji updater zapisuje również
`managed_sha256` dla każdego zarządzanego pliku; nie nadpisuje żadnego
niepowiązanego pliku klienta.

Po instalacji przycisk **Uruchom grę** automatycznie uruchamia wbudowany w
updater launcher PE32 x86. Launcher jest weryfikowany SHA256, dostarczany
wewnątrz updatera i ładuje wyłącznie zainstalowany AutoLoot335.dll
z wybranego katalogu gry. Updater sprawdza hash exe, DLL i dlls.txt
względem zapisanej instalacji; przy niezgodności nie uruchamia gry bez
aktywnego modułu, tylko wymaga **Sprawdź / napraw**. Nie pobieraj ani
nie instaluj osobnych paczek.

Przycisk **Natywny AutoLoot TEST** pozostaje wyłącznie dla starszego,
odizolowanego eksperymentu: odmawia uruchomienia, jeśli katalog gry ma
już zarejestrowany AutoLoot335.dll, żeby nie uruchamiać dwóch hooków.
STABLE/main nie zmienia się bez oddzielnej akceptacji.

## 0.3.9-335 — AutoLoot sent-message responsiveness in TEST/work
The embedded PE32 x86 launcher uses bounded SendMessageTimeout pulses plus
both game-thread message hooks. The registered AutoLoot335.dll is installed
only as part of the verified, exact-SHA TEST package and remains uninstallable
via the updater-managed rollback. STABLE/main remains unchanged.
