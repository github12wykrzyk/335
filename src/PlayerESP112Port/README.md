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
