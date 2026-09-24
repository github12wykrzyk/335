# PlayerESP335 — od nowa według działającego ESP 112

Źródło prawdy: github12wykrzyk/335, branch feature/player-esp-12340.
Wersja 0.4.0-112-gdi-rebuild-test, WoW.exe 3.3.5a build 12340, Windows x86.
Nie jest to wcześniejszy renderer Lua ani próba korekty jego współrzędnych.

## Analiza 112 vs 335

Odczytana implementacja 112:
\`src/WoWPlayerESP/WoWPlayerESP_v1_2_range_sweep.c\` na branchu
\`github12wykrzyk/wow112/parallel\`. Działa sekwencją:
(1) game-thread Object Manager i walidacja GUID/epoch,
(2) natywny klientowy WorldToScreen,
(3) **osobna** natywna funkcja DdcToNdc,
(4) NDC -> piksele klienta / ClientToScreen,
(5) niezależne przezroczyste, pulowane HWND/GDI, a nie FrameScript/Lua.
Wersja 112 ma pulę do 128 jednostek i nazwy/click-target, referencyjnie
\`project_world\` linie 955-977 i \`game_client_screen_rect\` 1397-1411.
Część sweep/GUID-cache/PvP nie została jeszcze odtworzona na klienta 335
i **nie wolno** oznaczać nowego modułu jako kompletnej kopii 112.

Stary 335:
- pomijał native DdcToNdc albo zastępował go dzieleniem raw XY przez
  +0x64/+0x70 (clip) lub +0x330/+0x33c (render);
- umieszczał etykiety przez UIParent / WorldFrame, osobną skalę Lua;
- miał D3D9 dummy EndScene 0 klatek i nakładkę z innej przestrzeni.

Nowy 335:
- sprawdza hash dokładnej lokalnej binarki EXE i prologi funkcji.
- natywny 12340 W2S: 0x004F6D20, ECX=worldframe, 3 argumenty;
  **nie** przenosi adresu 112 0x00483EE0.
- natywny 12340 DdcToNdc: 0x0047BFF0, cdecl
  (rawX, rawY, *ndcX, *ndcY), funkcja zbadana na oryginalnym Wow.exe
  (mul global 0xAC0CB4 / 0xAC0CB8, ret bez czyszczenia stosu).
  **Nie** używa 112 0x0041ADE0 ani dzielenia przez clip rect.
- przenosi 112 pikselową geometrię, 360x58, TOP_PAD=36,
  per-label layered transparent popup \`CreateWindowExA\`.
- 32 etykiety, najbliższe do 40 yd, obejmuje NPC i graczy; Ctrl
  scanner z obecnego 335 (natywna pozycja, GUID, health), bez stale cache.
- \`Insert\` przełącza widoczność. GDI HWND wymaga okna gry w trybie
  okienkowym lub bezramkowym; ekskluzywny fullscreen nie jest
  weryfikowany i nie może być reklamowany jako wspierany.
- Brak nazw NPC/graczy i kliknięcia target: wymagają potwierdzonego
  ABI 12340. NPC_ALL jest diagnostyczny. Nie podszywać nazw/relacji.

\`PlayerESP.jsonl\` dostaje \`backend=112-gdi\` i próbki:
\`native_12340_ddc\`, raw XY, natywne NDC, dwa globalne mnożniki,
piksele i rozmiar okna klienta. Nie zapisuje GUID ani współrzędnych
postaci. Jeśli \`overlay_create_errors>0\` albo \`projection_ok=0\`,
nie przypisywać tego do UI/Lua.

Nie aktywować jednocześnie starego PlayerESP335.dll. Loader ma ładować
wyłącznie jedną manifestową bibliotekę PlayerESP i żadnego drugiego
niezależnego D3D9 hooka.
Testy x86 i FINAL_PACKAGE PASS dowodzą zgodności build/manifest, NIE
rzeczywistego pokrycia etykiet na modelach. Wymagany test w grze na
identycznym SHA z trybem okienkowym/bezramkowym, 1 NPC i zoom/obrót.

## 2026-09-24: poprawka po rzeczywistym logu PlayerESP(2).jsonl

Wskazano przyczynę pomyłki portowania 112: dla przypiętego klienta 12340 funkcja W2S `0x004F6D20` **już wywołuje** natywne `0x0047BFF0` wewnątrz (instrukcja `0x004F6E45`). W poprzednim ESP112Port kod wywoływał `0x0047BFF0` DRUGI RAZ. Z realnego logu `raw_xy=[0.18139,0.03538]` z `ddc_global_xy=[0.87158,0.49026]` druga konwersja dała `[0.15810,0.01734]` i po odwróceniu Y etykietę przy dolnej krawędzi. Poprawiony adapter używa jednego natywnego W2S, odwraca jedynie jednostki natywnego UI do pikseli przez podzielenie przez odpowiednio `0xAC0CB4`/`0xAC0CB8` i **nie odwraca osi Y**. Test natywnej geometrii zawiera dwie próbki z logu. Nowa telemetria używa `probe=native_12340_ui_once`; nie mieszać z poprzednim plikiem. Lokalizacja modelu w grze wciąż wymaga sprawdzenia exact-SHA, bo screenshot bez GUID i surowej pozycji obiektu nie daje absolutnego ground truth.

## Diagnostyka parowana po kolejnym błędnym screenie (TEST)

Najnowszy obraz nadal pokazuje etykiety nieprzypisane wizualnie do koboldów. `PlayerESP(3).jsonl` jest plikiem dopisywanym: ma tylko dwa rekordy `native_12340_ui_once`, bez GUID i pozycji NPC, więc nie umożliwia dopasowania niebieskiego napisu do widocznego modelu. Zamiast zgadywać skalę wprowadzono znakowanie 6 najbliższych jednostek: `NPC ABCD` oraz **zielony krzyżyk `ABCD`** oznaczający rzut pozycji stóp TEGO SAMEGO GUID. Zrzut ekranu pokazujący jednocześnie napis i krzyżyk rozstrzyga, czy błąd leży w W2S / pobranej bazowej pozycji, czy tylko w podniesieniu z +2.30 i umieszczeniu labela. Zapis `probe=paired_head_and_feet` co 5 s zawiera jeden identyfikator, obie pozycje klienta, obie surowe współrzędne i world XYZ stóp. Test natywnych danych w grze nadal potrzebny; nie ogłaszać rozwiązania, dopóki zielone krzyże nie pokrywają się ze stopami przypisanych NPC. Pozostałe moduły, klient i main bez zmian.

## 2026-09-24: Y-native WoW vs top-left Windows (log PlayerESP(4))

Ponowna analiza pokazuje, że poprzednie stwierdzenie 'nie odwraca osi Y' było błędne. W 12340 WorldToScreen już wykonuje DdcToNdc jeden raz, lecz zwraca współrzędną Y rosnącą od DOŁU (UI/World), podczas gdy popup Windows interpretuje Y od GÓRY. Zewnętrzny opis WoW WorldToScreen w leifan8440/minibot-wow/API.md (sekcja In-World Vision) potwierdza dolno-lewy początek NDC; nasze parowane próbki z identycznym GUID to potwierdzają: 0D70 bazowy punkt feet y=335, head po podniesieniu +2.30 y=697 w starym renderze, czyli głowa była NIŻEJ na ekranie. Po korekcie Y_windows = (1-Y_ui/native_scale_y)*H, head y≈743, feet y≈1105 przy H=1440; odległość i X pozostają bez zmian. Nie wolno wywoływać DDC dwukrotnie. Parowane zielone krzyżyki nadal tylko diagnostyczne: jeśli po korekcie stopy lub etykiety rozmijają się z konkretną jednostką, trzeba zweryfikować wysokość modelu z binarki (stałe +2.30 jest przybliżeniem), a nie kolejną skalę. Weryfikacja rzeczywistego rozmieszczenia nadal wymaga testu w grze.

## 2026-09-24: zredukowanie szarpania przy ruchu kamery

Diagnoza kodu: poprzedni drive działał maksymalnie co 50 ms (20 Hz), niezależnie od liczby klatek gry; pobierał snapshot, sortował po odległości, po czym przypisywał indeks listy jako identyfikator okna. Gdy dwa NPC zmieniały kolejność, identyczny popup zmieniał target GUID i skakał na ekranie. Naprawa: nowy esp112_slots z trwałym GUID->slot (32 HWND), aż do eksplicytnego zwolnienia / zmiany world_epoch; sortowanie odległości nadal dobiera kto zostaje wyświetlony, ale nie zmienia HWND żyjących etykiet. Rzutowanie odświeża się co około 16 ms z aktualnej kamery; pełny, weryfikowany snapshot Object Manager co około 50 ms. Snapshot odrzucany natychmiast przy zmianie epoch, więc nie utrzymuje stale targetów przez relog. Pozostałe sloty są ukrywane maską zajętości, zamiast zakładać przedział indeksów [0..drawn). SetWindowPos nie przełącza już TOPMOST kolejno między 32 etykietami, co dodatkowo ogranicza szarpanie kompozytora. Dla game-thread hooks aktualna liczba wywołań gry nadal ogranicza realną częstotliwość projekcji: 16 ms jest granicą docelową, a nie gwarancją 60 FPS. Nie wprowadzono zgadywanego wygładzania z opóźnieniem za kamerą. Po TEST w grze można ocenić synchronizację z natywnym render frame i w razie konieczności przenieść wyłącznie aktualizację nakładki do render callback bez dodania konfliktowego D3D9 hooka. Grafika/krzyżyki diagnostyczne pozostają aktywne do potwierdzenia stabilności. main bez zmian.

Test regresyjny paired head/feet zaktualizowano po zmianie indeksowania HWND: wyświetlanie krzyża musi odwoływać się do przypisanego do GUID slotu, a nie do pozycji na posortowanej liście. Dokładny wynik w grze wymaga nowego SHA TEST.

## 2026-09-24: ESP steady-pacing + adaptive per-GUID low-pass TEST

Po zgłoszeniu szarpania: poprzednie 16 ms było **minimalną przerwą** między wywołaniami callbacków WH_GETMESSAGE/WH_CALLWNDPROC, a nie gwarancją regularnych aktualizacji. W nowym module na zweryfikowanym wątku gry przy aktywnym oknie uruchamiany jest thread-owned `SetTimer(NULL,0,16,NULL)`; WM_TIMER trafia do istniejącej pompy komunikatów bez nowego hooka lub nadpisywania kodu klienta. Timer jest wyłączany przy ukryciu ESP, utracie focus i zatrzymaniu modułu. Timer WM_TIMER ma niski priorytet; aktualizacja w grze może nadal być rzadsza niż 60/s. `esp112_motion` jest lekkim, adaptacyjnym filtrem 2D przypisanym do stable GUID->slot: tłumi drobny jitter przy nieruchomym kadrze; przy szybkim obrocie kamery zwiększa szybkość reakcji i natychmiast resetuje się po przeskoku >300px, pauzie >120ms, zmianie GUID oraz world_epoch. Nie wprowadza opóźnienia przez wieloklatkową kolejkę ani predykcji teleportów.

Przestarzałe zielone znaczniki stóp (do 6 dodatkowych HWND i 6 dodatkowych W2S co 16ms) są domyślnie wyłączone. `Ctrl+Shift+Insert` przełącza tryb diagnostyczny, zwykły `Insert` pokazuje/ukrywa ESP. W `PlayerESP.jsonl` nowy rekord `probe=cadence` co około 5 sekund podaje pomierzone `projection_hz`, `scan_hz`, `timer_wakeups`, `max_tick_gap_ms` oraz `debug_pairs`. Jeżeli mimo poprawy dalej występuje mikroprzycięcie, kolejny krok to **jeden renderer bezpośrednio zsynchronizowany z klatką gry**, bo osobne GDI HWND nie mają gwarancji VSync; nie wdrażać drugiego niezależnego D3D hooka kolidującego z loaderem/AutoLoot. Nowy test uruchomienia w grze z exact-SHA wymagany; build x86 sam nie potwierdza smoothness.

## 2026-09-24: pojedynczy renderer zsynchronizowany z klatką WoW (TEST, niezweryfikowany w grze)

**Jedyny właściciel** `PlayerESP335.dll` próbuje podpiąć natywne D3D9 `IDirect3DDevice9::EndScene` przez źródłowo przypiętą bibliotekę MinHook x86, bez drugiej DLL lub hooka w AutoLoot/loaderze. Zabezpieczenie odrzuca wcześniej przejęty entrypoint, inne okno, obcy wątek renderowania oraz viewport niezgodny z obszarem klienta; w tych sytuacjach poprzedni sprawdzony GDI pozostaje aktywny. Nie wolno deklarować, że D3D działa w grze przed dowodem `probe=frame_backend` z `frame_active=1`, `render_frames>0` i widocznymi etykietami. Właściwy klient 12340 pozostaje przypięty przez hash i prologi. Gdy trzy kolejne zweryfikowane klatki tego samego urządzenia i wątku gry zostaną przechwycone, GDI zostaje ukryte i timer zatrzymany. Pozostałe etykiety są tworzone z natywnego 12340 W2S podczas aktualnej klatki, bez dodatkowego smoothingu/zatrzymywania 16 ms, i składane jako jedno przejście `esp112_frame_draw` w bieżącym `EndScene` przez D3D9. Dynamicznych zasobów `D3DPOOL_DEFAULT` nie ma, stan D3D jest zapisywany/przywracany StateBlock. Reset/utrata urządzenia i brak zweryfikowanego render callback przez >2s przełączają na GDI. Sterowanie `Insert`, `Ctrl+Shift+Insert` zostaje; w D3D testowej wersji mały czcionkowy HUD/nazwy są inne od Windows GDI i wymagają akceptacji wizualnej. Windows x86 CI/FULL PACKAGE potwierdzają kompilację i spójność, **nie** przechwycenie klatek klienta. Log `PlayerESP.jsonl`: `probe=frame_backend` (`hook_installed`, `verified_callbacks`, `rejected_callbacks`, `foreign_thread`, `frame_active`, `render_frames`, `rendered_labels`, `install_errors`, `fallbacks`). Jeśli `frame_active=0`, nie twierdzić że wdrożono płynny rendering – aktywny jest fallback GDI i potrzebny osobny audyt aktualnego urządzenia gry. `MinHook` jest vendored z TsudaKageyu/minhook SHA 8af6b4acae5a9388fd742b56fa79ece89d96f823, BSD-2-Clause – pełna licencja w `src/ThirdParty/MinHook/LICENSE.txt`. Nie promować na main bez akceptacji gry.

## Poprawka testowa: migotanie renderera po przełączeniu na D3D9

Po teście użytkownika etykiety renderują się w grze, ale szybko migoczą. Bez raportu nie jest potwierdzona pojedyncza przyczyna. Poprzedni hook rysował w `EndScene`, który może wystąpić przed dalszymi przebiegami renderera i nie gwarantuje, że obraz zostanie zaraz przedstawiony. Nowy pojedynczy owner przechwytuje `IDirect3DDevice9::Present` (vtable index 17) zamiast `EndScene` (42), na tym samym zweryfikowanym device/HWND/thread; tworzy własną nie-zagnieżdżoną parę BeginScene/EndScene bezpośrednio przed oryginalnym Present. Jeżeli BeginScene odmawia, nie wykonuje callbacku i zapisuje `scene_failures`; nie forsuje gry ani nie deklaruje aktywnego renderera. Liczniki `scene_submitted`, `draw_failures`, `frame_disabled` w `frame_backend` służą do rozróżniania braku klatek, nieudanej sceny i nieudanego draw. Trzy kolejne nieskuteczne próby narysowania widocznych NPC blokują backend D3D9 dla bieżącej sesji, pozostawiając tylko sprawdzony GDI; przy przerwie klatek >2s następna próba zmiany backendu dopiero po 10s, aby nie oscylować co 2s. Nie używa dwóch hooków (Present zastępuje EndScene). Zasoby D3D pozostają per-frame, ze StateBlock restore; brak współdzielonej zmiany w AutoLoot i loaderze. To jest eksperyment na dokładnym klientcie 12340: poprawny build i smoke nie są dowodem wyeliminowania migotania.

Poprawka: zero faktycznie rysowanych etykiet (wszystkie NPC poza viewportem) nie jest awaria D3D. esp112_frame_draw_success() odroznia udany pusty draw od bledu API. Fallback po trzech rzeczywistych bledach; nie przy chwilowym zejściu NPC poza ekran.

## 2026-09-24: korekta mikrodrgań obrazu D3D9, bez zmiany działającego Present

Użytkownik potwierdził, że napisy **nie migają**, jedynie lekko szarpią. Po analizie obecnego toru znaleziono przedwczesne zaokrąglenie wyniku projekcji do całkowitych pikseli w `esp112_ui_to_client`, po którym wynik był ponownie konwertowany z int do float przy przekazaniu do renderera D3D9. Nowy `esp112_ui_to_client_precise` zachowuje współrzędne float na potrzeby D3D9, pozostawiając nadal oryginalne zaokrąglone int dla GDI i debug-krzyżyków. Test geometrii sprawdza, czy przemieszczenie projekcji o 0,25 piksela dociera do renderera bez obcięcia. Nie zmienia się hook Present, warunek przełączenia fallback, native 12340 W2S, skaner ani kadencja snapshotów 50 ms. D3D9 raster może nadal prezentować drobne skoki wielkości pojedynczych pikseli, a poruszające się NPC mogą być ograniczane przez tempo skanera; pełny brak szarpania nie jest gwarantowany przed testem exact-SHA w grze. Nie wzmacniać bez pomiaru filtra kamery, bo powoduje opóźnienie śledzenia.
