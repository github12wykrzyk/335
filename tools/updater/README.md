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

## 0.3.8-335-epoch-test — isolated EpochConnection startup loader

Branch `feature/loader-12340` only. A dedicated Windows 2022 CI job compiles the
actual Wow335Loader.dll as PE32 x86 and patches an isolated copy of the exact
160768-byte EpochConnection.dll from this repository. It checks original and
new export ordinals, import descriptors, TLS, relocations, section contents,
extra section and SHA256, and publishes the four-file TEST pair ONLY if checks pass.
This is a static-gated TEST artifact, not FINAL_PACKAGE: PASS and not proof that
the original network tunnel behaves correctly at runtime.

The updater's **Epoch Loader TEST** button obtains the latest successful exact-HEAD
artifact from `github12wykrzyk/335`, confirms its four-file allowlist,
manifest, SHA256 and the exact local Wow.exe and original EpochConnection.dll,
then backs up EpochConnection.dll and installs the two verified DLLs. Existing
unmanaged Wow335Loader.dll or nonempty dlls.txt block installation; no unknown
game files are overwritten. The adjacent **Przywróć Epoch DLL** button verifies
installed byte hashes, restores the exact original and removes managed test files.
Normal TEST(work) and STABLE(main) package channels are unchanged.

The updater owns the approved dlls.txt order and currently creates an **empty**
list because `runtime/current.json` declares no validated game modules.
Wow335Loader.dll also creates the empty file if it is absent at startup; it never
searches the game directory or loads unregistered DLLs automatically.
The UI **Aktywne DLL / kolejność** reports the current empty set; enabling
game-feature DLLs requires their own exact-SHA registry, dependency/order gate
and a later compatible uploader workflow. The user must not hand-edit dlls.txt.

When installed, the normal **Uruchom grę** button verifies every managed DLL
and the exact Wow.exe hash before launching. If any bytes differ, it refuses to
start under this TEST state. This experiment never modifies Wow.exe or main/work.

## 0.3.9-335-epoch-test — managed work AutoLoot interop

The initial Epoch TEST installer incorrectly rejected the already managed,
non-empty dlls.txt from work. This isolated fix accepts exactly the registered
work/149a2523f8068e407a8fc74e058b4f24c81d21e5 AutoLoot335.dll only:
it checks the existing managed updater state, listed order and both on-disk
SHA256 values against that pinned work runtime; any unknown or modified file
blocks installation without changing the client. It preserves the existing
AutoLoot335.dll and dlls.txt exactly; the original EpochConnection.dll is
backed up and the exact-CI test pair replaces only the network DLL and
creates the additional loader DLL. Rollback restores the original EpochConnection
and pre-install dlls.txt, leaving the work AutoLoot bytes untouched.

Loading the work AutoLoot DLL is not sufficient to activate its native game
logic. The loader now additionally installs its documented WH_GETMESSAGE hook
on the actual game window thread and posts its enable / tick messages,
mirroring the verified work launcher ABI. This combined execution path is
an **untested compatibility experiment**: CI PE/hashes and compilation do not
prove that game networking and AutoLoot coexist. Wow335Loader.log records
LOADED/HOOK_ENABLED/HOOK_FAILED/HOOK_STOPPED. No other module receives such
implicit hook activation. Normal work package updates and work rollback are
blocked while the isolated Epoch TEST install is active to protect both sets
of managed files. Use the separate Epoch rollback first.

Unknown or future work DLL versions are not implicitly trusted: the interop
pin needs a new verified compatibility set with dependency/resource review.
The current feature runtime manifest still does not claim a full game package.

## 0.3.10-335-epoch-test — legacy work state and diagnostic stage

The feature updater now handles the precise work/149a2523, successful run
35855470746, installed by an older updater that omitted managed_sha256 in
installed.json. This compatibility path still requires an installed state
from the exact work SHA, exact artifact name and run id, managed file names,
an allowlisted one-module dlls.txt order, and the pinned SHA256 of the actual
AutoLoot335.dll bytes. Any malformed hash map or unknown work installation
remains a hard error, and no client file is modified on preflight failure.

Epoch TEST errors include the exact phase (local work manifest, branch HEAD,
runs, artifacts, pair metadata, pre-install validation, backup/install).
Do not log token values or raw GitHub API payloads.

## 0.3.11-335-epoch-test — Git branch SHA validator repair

Epoch TEST was incorrectly rejecting a valid 40-character Git commit SHA
with the 64-character SHA256 file-digest validator, both when reading GitHub
branch HEAD and when reading the locally installed Epoch TEST state.
The updater now uses a dedicated 40-lowercase-hex Git SHA check and retains
64-hex SHA256 validation for all binary/dlls.txt hashes.
An executable Windows x86 regression test covers real Git SHA length, file
SHA256 length, malformed hex and case handling. No changes to game DLL bytes.

## 0.3.12-335-epoch-test — trzy przyciski, jeden workflow

Główne okno testowego updatera zawiera tylko: Sprawdź, Aktualizuj,
Aktualizuj i uruchom. Dodatkowe przyciski eksperymentów AutoLoot i
Epoch, narzędzi i ręcznego rollbacku nie są wyświetlane. Kanał TEST
sprawdza dokładny HEAD feature/loader-12340 i najnowszy udany workflow
pary DLL; instalacja i kolejne aktualizacje Epoch odbywają się przez
podstawowe przyciski. Funkcja Aktualizuj i uruchom uruchamia grę
WYŁĄCZNIE po powodzeniu weryfikacji/instalacji. Aktualizacje
istniejącej pary zachowują oryginalny backup oraz istniejące
AutoLoot335.dll i dlls.txt, a przy niepowodzeniu próbują przywrócić
poprzednią parę zweryfikowanych bajtów. Lokalna historia backupów
nie jest kasowana. Ukrycie przycisków NIE oznacza promocji TEST
do STABLE ani FINAL_PACKAGE: PASS. Kanał STABLE pozostaje chroniony
przed nadpisaniem aktywnego eksperymentu.

Przed pełnym autonomicznym instalowaniem na czystej instalacji klienta
wymagany jest dopuszczony komplet zależnych DLL i manifest: bieżący
Epoch TEST integruje tylko zweryfikowany stan AutoLoot z work/149a2523.

## 0.3.13-335-epoch-test — bez osobnego przycisku aktualizacji updatera

Każda operacja Sprawdź, Aktualizuj lub Aktualizuj i uruchom na kanale
Epoch TEST najpierw sprawdza najnowszy udany workflow updatera na
dokładnym SHA brancha feature/loader-12340. Jeśli wersja aplikacji się
zmieniła, pobiera i weryfikuje nowy updater, restartuje aplikację,
a instalację DLL i uruchomienie gry wstrzymuje do ponownej akcji
użytkownika. Zachowano oryginalny weryfikowany SHA256,
Windows x86 bootstrap, kopię aktualnego updatera i odbudowę po błędzie.
Brak dodatkowego przycisku self-update na ekranie.

## 0.3.14-335-epoch-test — aktualizacja loadera i gry jednym przyciskiem
TEST: „Aktualizuj” najpierw uruchamia istniejący mechanizm self-update updatera, następnie instaluje zweryfikowaną parę EpochConnection.dll/Wow335Loader.dll i migruje wyłącznie zarządzany AutoLoot335.dll 1.0.0-test do dokładnych bajtów 1.0.1-test z work/3cbc14b. Nowy loader instaluje WH_GETMESSAGE i WH_CALLWNDPROC oraz wysyła ograniczone czasowo impulsy do wątku gry co 40 ms. Nowe bajty AutoLoot są zagnieżdżone jako zasób updatera i weryfikowane SHA256/PE32 x86 w Actions. Lokalny stan starego AutoLoot i Epoch zachowuje backup/rollback; obce DLL nie są nadpisywane. „Sprawdź” rozróżnia aktualny loader i aktualny AutoLoot. Kanał STABLE/main bez zmian. Gra i łączność sieciowa wymagają testu konkretnego SHA.
