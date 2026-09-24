# PlayerESP335 — wczesny rdzeń, jeszcze NIE moduł gry

Implementacja `player_esp_core.c/.h` jest oryginalną, przenośną logiką
snapshotów GUID, filtrów Horde/Alliance/hostile/BG, projekcji współrzędnych
i wyboru GUID kliknięciem w etykietę. Zawiera testy deterministyczne w
`tests/player_esp_core_harness.c`; nie zawiera odczytu pamięci klienta,
hooków, renderera, obsługi myszy ani kodu wywołującego target w WoW.
Brak `PlayerESP335.dll`, rejestracji aktywnej DLL i paczki TEST.

Połączenie z WoW wymaga niezależnie zweryfikowanego adaptera dokładnego
`Wow.exe` SHA256 z `runtime/client_exe_target.json`. Dane kamery są
przekazywane jako spójna macierz view-projection dla bieżącej klatki w
konwencji Direct3D [0,w] i viewport — żadne offsety ConsoleXP nie są
automatycznie uznane za zgodne. Epoch zmienia się przy przejściu między
światami, instancjami i logowaniu. Kliknięcie zwraca wyłącznie GUID:
adapter ma ponownie znaleźć żywy obiekt na właściwym wątku gry i
arbitrować targeting z AutoPickPocket, zanim wywoła natywny target.

Do wykonania przed aktywacją:
1. potwierdzić dokładny ABI dla listy graczy, nazw, relacji PvP, HP,
   kamery i targetowania dla przypiętego klienta, bez zapożyczania offsetów;
2. dodać jednego właściciela renderowania D3D9 (jeśli backend potwierdzony)
   i wejścia z zachowaniem stanu i odtwarzaniem zasobów po resecie urządzenia;
3. zintegrować `W335_MessageId/W335_HookProc/W335_CallWndProc` z loaderem,
   zadeklarować źródła, zależności, hooki i arbitraż w module_registry;
4. wykonać rzeczywisty build PE32 x86, zarejestrować jego dokładny hash
   w runtime/current.json, przejść bramki TEST i dopiero przetestować w grze.

Zasoby ConsoleXP są tylko referencją. Nie ładować ZIP, DLL, patchera ani
nie podmieniać Wow.exe. Szczegóły: docs/CONSOLEXP_REFERENCE_ESP.md.

## Etap 2 — skaner Object Manager

`player_esp_scanner.c/.h` zawierają obserwator listy obiektów dla
znanego układu 12340, z callbackami weryfikującymi SHA klienta, ABI,
wątek gry, aktualny epoch świata, bezpieczne odczyty i metadane gracza.
Skaner nie modyfikuje targetu/ruchu/EXE, nie instaluje hooków i nie
przechowuje adresów obiektów w snapshotach. Przy błędzie unieważnia stan.
Weryfikacja jest wykonywana na symulowanej pamięci; poprawność ABI
na dokładnym `Wow.exe` i połączenie z loaderem pozostają do wykonania.

## Etap 3: rzeczywisty host x86 (nadal nie wizualne ESP)

`player_esp_win32_host.c` eksportuje W335_* i korzysta z loadera. Przy inicjalizacji sprawdza SHA klienta i prolog pozycji, odczytuje zdrowie/max zdrowie i enumeruje graczy na watku gry. Nieznane frakcja/klasa/BG sa zerowane, nie zgadywane. Log zbiorczy (bez GUID i nazw): `.wow335_debug/PlayerESP.jsonl` co 5 s. Brak drugiego loadera, patchera i hooka D3D. Workflow kompiluje realna PE32 x86 DLL i sprawdza eksporty; publikuje wylacznie raport builda, NIE DLL, bo nadal brak renderera/targetowania, rejestracji aktywnego runtime i testu w grze. SHA/prolog nie sa dowodem pelnego ABI. Nie instalowac tej DLL samodzielnie.

## Etap 4: camera + D3D9 marker prototype

Host odczytuje live worldFrame/activeCamera, sprawdza wektory i zakresy FoV/near/far oraz oblicza macierz projekcji w `player_esp_camera.c`. Renderer `player_esp_d3d9.c` zakłada **wyłączną własność** slotu 42 vtable EndScene D3D9 i buduje markery: punkt gracza, pasek HP, dystans cyframi. Wsparcie wymaga aktywnego backendu D3D9 i współdzielonego vtable HAL; w razie błędu kamery, niezainicjalizowanego renderera czy zmiany epoch nie rysuje nic. Nie ma kliknięcia / targetowania ani nazw / klasy (metadane nieweryfikowane).

Z uwagi na konflikt z dowolnym innym właścicielem EndScene (np. niezależnym ConsoleXP / overlay), nie wolno równocześnie uruchamiać innych niezależnych hooków D3D9. Raport zawiera liczniki render_frames/camera_ok/camera_bad/markers, ale tylko rzeczywisty test w kliencie potwierdzi poprawną pozycję nakładki. DLL nie jest w aktywnym runtime i nie jest paczką gry.

## ESP GUI / diagnosis (experimental)

GUI is drawn in-game by the existing D3D9 EndScene owner, not by a second launcher. INSERT toggles the menu; click checkboxes: ESP ON, PLAYERS ALL, HORDE, ALLIANCE, HOSTILE, BG ENEMY, NPC ALL, NPC HOSTILE, UNKNOWN. NPC markers are blue. World-thread scanner and camera are copied to a locked immutable frame so a different D3D render thread does not read client memory or race on the object list. The UI is rendered even if scanner or camera cannot provide a valid frame. HUD counters SCAN/P/N/C/M/DROP and .wow335_debug/PlayerESP.jsonl help distinguish a camera/scan/render failure. MAXHEALTH is descriptor index 32 (25 is mana).

Caution: faction is read from 12340 player race; it does not determine mixed-BG hostility. BG ENEMY and HOSTILE filters only match confirmed runtime reaction/team, which still needs a validated client ABI; unclassified players can be shown with PLAYERS ALL/UNKNOWN. NPC HOSTILE only matches explicitly identified monster faction templates 14/16; NPC ALL covers all scanned units. Do not present BG hostility as working before in-game proof.

## TEST naprawczy: INSERT bez zależności od skanera

Wcześniejszy błąd: `control` wstrzymywał GUI i render D3D9, jeśli `esp335_scanner_bind` nie przeszedł jednokrotnej weryfikacji. Teraz najpierw uruchamia się osobny interfejs D3D9, a skaner w razie błędu ponawia próbę co 5 sekund. GUI rysuje się także przy `scan_ok=0`. Diagnostyka `.wow335_debug/PlayerESP.jsonl` zawiera `render_attempts/render_failures/render_frames`, `init_attempts/sha_rejects/layout_rejects`, `hook_calls/insert_events/gui_open`. Ciągle nie ma dowodu poprawnego działania w grze; jeśli nie ma GUI, porównać ostatnie wiersze logu z tymi licznikami.

## TEST fix Insert: polling na heartbeat

Próba wciśnięcia Insert jest odczytywana niezależnie od WM_KEYUP za pomocą GetAsyncKeyState na wątku okna gry, gdy okno ma fokus; jeden key-down = jedno przełączenie, a WM_KEYUP obsługuje szybkie tapnięcie bez podwójnego toggla. GUI nadal rysuje się niezależnie od powodzenia skanera. W raporcie szukać insert_polls/gui_toggles/insert_events/render_frames/render_failures; brak wzrostu polls oznacza problem pulsu/fokusu, toggles bez render_frames oznacza osobny problem nakładki D3D9.


## Fallback native WoW UI from ESP log 2026-09-24

Observed uploaded report: render_installed=1, render_frames=0; Insert toggles increased and 101 NPCs were scanned. A dummy-device D3D9 vtable can appear installed but never receive the game's EndScene calls. This is a **runtime failure**, not an Insert failure. The visual TEST now uses WoW 3.3.5 native FrameScript/UIParent frames through a guarded AutoLoot-owned AL335_ExecuteUiScript export; do not independently invoke the client FrameScript address from ESP. In-game UI and 2D markers are updated from the same game-thread scan/camera pipeline. The old D3D9 prototype is retained as reference code only; it must not be installed in this runtime path. Mixed-BG hostility remains unknown until validated from an exact-client relation/team API.

## TEST NPC / panel w grze

W analizowanym miejscu brak graczy jest spodziewany; testuj NPC ALL (domyślnie włączone) i panel widoczny po starcie. Raport z gry pokazał render_frames=0 pomimo render_installed=1, dlatego D3D9 dummy-vtable NIE jest już instalowane. ESP buduje WoW-native ramkę Lua przez zweryfikowanego właściciela FrameScript AutoLoot; nowe pola raportu to lua_gate_missing, lua_init_attempts, lua_init_ok, lua_updates, lua_update_errors i lua_ready. Obie DLL kompilowane i rejestrowane atomowo. Filtry BG ENEMY/HOSTILE nie zostały zweryfikowane na mieszanym BG.

## Korekta TEST po zrzucie ekranu 2026-09-24

Na zrzucie 154 NPC byly skanowane w zasiegu lokalizacji i poprzednie ESP wybieralo pierwsze 48 widocznych na liscie obiektow (niekoniecznie najblizsze), do 120 yd; znaczniki zachodzily na siebie i na panel. Teraz C wybiera i sortuje widoczne cele od najblizszego, kotwica nad pozycja obiektu to orientacyjnie +1.60 (NPC) / +2.30 (gracz) wzdluz Z, limit wynosi 24. Native WoW GUI domyslnie pokazuje do 40 yd (przycisk RANGE zmienia na 80,120,25), odrzuca bliskie sobie etykiety i zaslaniane przez otwarty panel. Sa to korekty czytelnosci, nie dowod dokladnej projekcji/LoS: natywny W2S z klienta 5875 z ESP 112 nie jest poprawnym adresem w kliencie 12340. Dla dalszej diagnostyki potrzeba sprawdzic oznaczenie tej samej jednostki na ekranie przy obrocie i zmianie zoomu; kamera ConsoleXP jest tylko hipoteza ABI, bez pelnej gry nie certyfikujemy zgodnosci z natywnymi nameplate'ami. Nie stosowac starszej paczki po aktualizacji branch.

## Etap 12340 native WorldToScreen (eksperymentalny TEST)

Po nieudanym odtworzeniu macierzy kamery z ConsoleXP implementacja używa teraz natywnego WorldToScreen klienta 12340: `0x004F6D20`, zbadana na przypiętym Wow.exe 12340 i wywołaniach kodu gry `0x5253A5` i `0x5253DA`. ABI: ECX=worldframe, argumenty world[3]*, result[3]*, clipflags*; callee usuwa 12 bajtow stosu. Wynik przeliczany z worldframe viewport +0x64/68/6c/70 do [0,1] i rysowany przez dotychczasowy jeden wspólny UIParent/AutoLoot FrameScript. Sprawdzane SHA EXE i caly prolog funkcji przed bind. Stara `esp335_camera_build` pozostaje tylko diagnostyką; nie wyznacza pozycji znacznika. Jest to przeniesienie natywnego mechanizmu WorldToScreen z ESP 112, z adresami i ABI klienta 335; pixel-perfect oraz odpowiednią konwencję współrzędnych należy potwierdzić w grze (np. dwa katy patrzenia na ten sam NPC).

## Zgłoszenie: wszystkie znaczniki przesunięte mimo natywnego W2S (2026-09-24)

Poprzedni FINAL_PACKAGE: PASS nie dowodzi prawidlowych pozycji w grze. Zidentyfikowany blad w poprzednim adapterze UI: wspolrzedne normalizowane z natywnego WorldFrame byly skalowane do calego UIParent zamiast do prostokata WorldFrame (ten drugi moze miec inny poczatek, wymiary i skale). Marker jest teraz dzieckiem WorldFrame i zakotwiczony wzgledem WorldFrame:BOTTOMLEFT; panel nadal jest UIParent. Nie zmieniono ABI natywnej projekcji ani nie nadpisano binarki gry. Dodany rekord `probe=native_w2s` do .wow335_debug/PlayerESP.jsonl co maksymalnie 5 s: raw_xy, clip_lbrt, render_lbrt z worldframe +0x330..0x33c, znormalizowane xy oraz rozmiar okna. Nie zawiera GUID ani wspolrzednych postaci. Jesli napis nadal nie trafia na model, nalezy wyslac raport z updatera po tescie TEGO SAMEGO SHA, a nie na slepo zmieniac znak osi lub mnoznik FoV. Pełne odtworzenie z 112 GDI layered HWND i jego natywny W2S 5875 nie moze zostac uznane za gotowe bez spójnego viewportu klienta 12340 i testu w grze.
