# ConsoleXP 1.0.1 → PlayerESP335: izolowana referencja

W repo work znajdują się dwa przekazane archiwa w
`reference/ConsoleXP/`. Są referencjami; nie są zależnościami loadera,
updatera, build_active, dlls.txt ani runtime/current.json.

## Provenance i audyt archiwów
`tools/audit_consolexp_reference.py` przypina ich dokładne identyfikatory
Git blob z odczytanego drzewa work, kontroluje integralność ZIP (CRC),
ścieżki, limity dekompresji, typy plików, obecność licencji i raportuje
potencjalnie użyteczne pliki źródłowe. Workflow
`.github/workflows/consolexp_reference_esp.yml` uruchamia ten audyt
bez wypakowywania na dysk i publikuje tylko raport JSON. Nie uruchamia
dostarczonych EXE/DLL, nie buduje z ich źródeł i nie patchuje klienta.
Zmiana obu archiwów wymaga jawnej weryfikacji i aktualizacji SHA w audycie.

## Analiza czytelnych plików oryginalnego upstreamu
Upstream: https://github.com/leoaviana/ConsoleXP ; licencja MIT
(Copyright 2025 leoaviana). Dokładna zgodność zawartości przekazanej
paczki 1.0.1 z aktualnym upstreamem wymaga porównania raportu archiwum
i właściwego źródłowego wydania; NIE zakładamy ich tożsamości.

- `src/ConsoleXP/Game.cpp`: viewport z D3D9, pozycja i macierz kamery,
  funkcja WorldToScreen. Wariant upstreamu zawiera arbitralne FoV * 0.6:
  nie używać go jako wzoru bez walidacji projekcji na dokładnym kliencie.
- `src/ConsoleXP/Targeting.cpp`: iteracja obiektów i wybór przez GUID;
  używać tylko koncepcji. W ESP nie utrzymywać długowiecznych wskaźników.
- `src/ConsoleXP/Hooks.cpp`: detour EndScene przez MinHook, własne
  hooki FrameScript i targetowania. NIE integrować drugiego hook chain:
  właściciel renderera i logical:targeting wymagają arbitrażu.
- `src/ConsoleXP/Camera.cpp`: modyfikacje pamięci kamery i detours;
  są niepotrzebne dla ESP tylko odczytującego pozycje.
- `src/ConsoleXPPatcher/`: patcher EXE; zabroniony w ścieżce projektu.
- `external/MinHook_134_lib/`: biblioteka z odrębną licencją.
  Nie kopiować ani linkować bez osobnego audytu licencji i hooków.

## Workflow wdrożenia ESP
Najpierw przygotować i przetestować przenośny core `src/PlayerESP/`,
następnie wyizolować adapter dla SHA-przypiętego klienta z interfejsem
obserwacji Object Managera (bez konfliktu z AutoLoot i AutoPickPocket).
Osobny wspólny właściciel renderowania 3D i wejścia obsługuje ESP
na wątku gry; pobiera spójny snapshot z bieżącej klatki i nie blokuje
ticków innego modułu. Przed rejestracją DLL: rzeczywisty PE32 x86,
odtwarzalny build, zgodne hash/manifest, test konfliktów hooków,
test przejścia do BG/arena, test po Alt+Tab/Reset D3D i kliknięcia GUID.
Żaden PASS samego audytu archiwum albo rdzenia nie oznacza pakietu gry.
