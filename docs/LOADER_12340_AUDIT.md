# Loader 12340 — audyt binarny (TEST, bez uruchamiania kodu)

Branch `feature/loader-12340` wyizolowano z `work` commit `18905f8c9d3ebc57588e12b1f2f76416572c2659`.

## Docelowy klient

Przypięty `Wow.exe`: wersja 3.3.5.12340, PE32 x86, 7 717 528 B, SHA256 `2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d`.
Narzędzie `tools/audit_loader_12340.py` odczytuje wyłącznie statyczne importy/znaczniki tego dokładnego pliku i nie uruchamia go.

Na commit `5af79cd4c01600f18818913510acea68288cd4a7`, workflow `Build 335 work candidate` run `35854572067` wykonał 35 testów z sukcesem. Faktyczne importy klienta obejmują `EpochConnection.dll` poprzez ordinal `#1`; ciągi `twloader.dll`, `func1`, `dlls.txt` nie występują w statycznym skanie jako tekst ASCII. To nie dowodzi istnienia ani nieistnienia innych mechanizmów dynamicznego ładowania.

## Przesłana przez użytkownika biblioteka EpochConnection.dll (referencja, NIE runtime 335)

- Nazwa: `EpochConnection.dll`, dokładny rozmiar: 160 768 B, SHA256: `9af04f7afd21b0bc93860d66e6ccdc96ccf1b039ea31871119a3deaa271352c3`.
- PE32 / i386 (`0x014c`), 8 sekcji; eksporty:
  - ordinal `1`: `EpochConnectionAnchor`, RVA `0x3870`;
  - ordinal `2`: `EpochConnectionCheck`, RVA `0x3880`;
  - ordinal `3`: `EpochConnectionStatus`, RVA `0x3a00`.
- Przypięty `Wow.exe` importuje ordinal `#1` biblioteki `EpochConnection.dll`; przesłana DLL udostępnia ten ordinal, co potwierdza zgodność *nazwy i numeru eksportu*, nie potwierdza ABI i działania w grze.
- Importy DLL obejmują `WS2_32.dll`, `WINHTTP.dll`, `KERNEL32.dll`, w tym `LoadLibraryExW` i `GetProcAddress`. Sam import funkcji ładowania nie czyni tego modułu ogólnym loaderem innych DLL.
- Statyczny skan ASCII/UTF-16LE nie wykazał `dlls.txt`, `twloader.dll`, `func1`, `plugin` ani `loader`. Nie wyklucza to generowanych lub zaszyfrowanych nazw, ale *nie ma dowodu*, że ten plik ładuje `dlls.txt`.
- Przesłany `EpochConnection.log` zawiera komunikaty `In-process connection ready`, `Login tunnel opened/closed`, `World tunnel opened/closed`; jest to materiał diagnostyczny tunelu sieciowego, bez wpisów wskazujących odczyt `dlls.txt` lub wynik ładowania innych DLL.

## Granice i dalsza weryfikacja

`EpochConnection.dll` jest istniejącą zależnością przypiętego EXE. NIE zastępować, nie usuwać ani nie przemianowywać jej na `twloader.dll`; nie publikować nadpisującej jej biblioteki proxy bez osobnej oceny kompatybilności. Ten etap nie zmienia `Wow.exe`, nie uruchamia kodu użytkownika i nie jest grą gotową do testu. W `runtime/current.json` brak aktywnych modułów; `FINAL_PACKAGE: PASS` i test gry nie zostały uzyskane.

Dalsze ewentualne prace wymagają potwierdzenia zgodnego mechanizmu startu na *dokładnym* build 12340 x86, odseparowania logicznych funkcji tunelu od nowego loadera, deklaracji własności zasobów/dependencies wszystkich modułów oraz testu w grze na konkretnym SHA. Nie kopiować offsetów, ABI ani hooków WoW 1.12.

## Etap 2: izolowany import startup-loader, bez promocji

`src/Loader12340/Wow335Loader.c` — samodzielna biblioteka Windows x86. Odczytuje listę `dlls.txt` wyłącznie w katalogu gry, wymaga prostych nazw plików, odrzuca nazwy z katalogami, duplikaty i wczytywanie `EpochConnection.dll` lub samego loadera. Prowadzi `Wow335Loader.log`. Ładuje w kolejności manifestu po odblokowaniu loader-lock. Nie implementuje updatera, ani nie stanowi dowodu zgodności modułów klientowych z 12340.

`tools/loader_patch_12340.py` tworzy *oddzielną kopię testową* EXE: zachowuje oryginalne importy (w tym `EpochConnection.dll` ordinal 1), dopisując drugą niezależną bibliotekę `Wow335Loader.dll` ordinal 1. Odrzuca niezgodny SHA klienta, pełny nagłówek sekcji, EXE z podpisem lub bound imports; nigdy nie nadpisuje canonical `Wow.exe`. Nie jest ogólnym patcherem klientów WoW — tylko dokładne bajty przypiętego EXE.

Workflow `.github/workflows/loader_12340_preview.yml` w izolowanym branchu buduje DLL MSVC x86, weryfikuje ordinal 1, wykonuje testy i próbę dopisania importu na dokładnym klientcie 12340. Wynik jest wyłącznie materiałem badawczym `NOT_GAME_PACKAGE`; nie jest przeznaczony do ręcznej instalacji ani aktualizacji kanału TEST/STABLE. Nie ma aktywnych DLL gry w runtime/current.json ani zintegrowanego updatera z wyborem i weryfikacją modułów. W następnej iteracji loader powinien wymagać tożsamości każdego modułu (hash, manifest z GitHuba) i startować tylko z kompatybilnym kompletem, zanim zostanie dopuszczony jako paczka gry.

## Etap 3 — analiza układu nagłówków PE (tylko odczyt)

Raport uzyskany na **dokładnym**, przypiętym klientcie 12340 w `Build 335 startup loader isolated TEST`, run `35857657722`, commit `dc353ea85082cddd70c5c97314232ed400c5f2cc`. Skrypt `tools/audit_pe_layout_12340.py` nie modyfikuje EXE. Wszystkie 44 testy jednostkowe przeszły, ale izolowany workflow loadera nadal zgłasza błąd przy próbie modyfikacji EXE — nie ma paczki gry.

- PE header `0x50`, optional header `0x68` (PE32, rozmiar `0xE0`); formalna tablica **7** sekcji od `0x148` do `0x260`, `SizeOfHeaders=0x400`, pierwszy `PointerToRawData=0x400`. Wstawienie ósmego opisu pod `0x260` nadpisałoby istniejące dane.
- `0x260–0x2F7`: niezerowe 53 bajty w zajętym obszarze do `0x400`. Fragmenty zawierają parametry odpowiadające sekcji `.data`, po których następują kopie opisów `.zdata`, `.tls` i `.rsrc`. Wyglądają jak pozostałość drugiego zestawu nagłówków; rzeczywisty nagłówek COFF **nie deklaruje** dodatkowych sekcji. Same wzorce nie dowodzą, że pozostałość jest nieużywana — nie usuwamy jej automatycznie.
- Aktywne sekcje wg COFF: `.text` RVA `0x1000` raw `0x400`; `.rdata` RVA `0x5DF000` raw `0x5DD800`; `.data` RVA `0x6B6000` raw `0x6B4000`; `.zdata` RVA `0x9D1000` raw `0x72CE00`; `.tls` RVA `0x9D2000` raw `0x72DE00`; `.rsrc` RVA `0x9D3000` raw `0x72E000`; `.detour` RVA `0x9FD000` raw `0x757C00`. Ostatnia sekcja kończy się w pliku na `0x75B000`.
- `DataDirectory[1] Import`: RVA `0xA000D0`, rozmiar 380, odczyt mapuje się na raw `0x75ACD0` **wewnątrz istniejącej sekcji `.detour`**; nowa tabela importów nie może ignorować już podpiętego klienta. `DataDirectory[12] IAT` RVA `0x5DF000`, raw `0x5DD800`, rozmiar 1948.
- `DataDirectory[4] Security`: wskaźnik **pliku** `0x757C00`, długość `4760`, pokrywa początek raw `.detour`; za ostatnią sekcją istnieje overlay długości **również 4760 B**. W przeciwieństwie do pozostałych katalogów ten wskaźnik jest pozycją w pliku, nie RVA. Sama niezerowa wartość nie potwierdza poprawnego podpisu — obydwa obszary wymagają odczytu nagłówka `WIN_CERTIFICATE`, zanim wolno ustalić regułę korekty.
- `DataDirectory[6] Debug`: RVA `0x5E0B10`, raw `0x5DF310`, rozmiar 28; jego `PointerToRawData` wskazuje `0x6AD788`. `DataDirectory[9] TLS`: RVA `0x6AEF70`, raw `0x6AD770`, rozmiar 24, z `AddressOfCallbacks=0x9E0AF4` jako **VA**, nie file offset. Te wskaźniki i wywołania TLS trzeba zachować.
- `AddressOfEntryPoint=0x1000` (RVA), `ImageBase=0x400000`, `SizeOfImage=0xA01000`, `FileAlignment=0x200`, `SectionAlignment=0x1000`. `PointerToSymbolTable=0`; katalog `BoundImport=0`; katalog relokacji bazowych=0. Przy zmianie surowych offsetów aktualizacji wymagają `PointerToRawData` sekcji, ewentualne wskaźniki relokacji/numerów linii, wskaźniki debugowania, COFF oraz **Security file offset**. Same RVA importów, zasobów, TLS i IAT nie są file offsetami; nie przesuwa się ich automatycznie razem z raw sekcjami.

Dopóki nie potwierdzono funkcji dodatkowych danych nagłówka i spójności Security/overlay oraz zgodności nowej kopii PE z loaderem Windows x86, **nie publikować ani nie podmieniać EXE**. Brak integracji z updaterem i `FINAL_PACKAGE: PASS`.
