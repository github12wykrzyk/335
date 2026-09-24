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
