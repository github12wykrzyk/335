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
