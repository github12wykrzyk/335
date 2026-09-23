# EpochConnection.dll — izolowana próba dodania importu loadera, 12340 x86

Wybór użytkownika: nie modyfikować więcej canonical Wow.exe; sprawdzić dodanie ładowania modułów do istniejącej zależności EpochConnection.dll. Nie używać wariantu proxy bez osobnej walidacji.

## Dane wejściowe i ograniczenia
- Użytkownik przesłał `EpochConnection.dll` (PE32 i386, 160768 B, SHA256 `9af04f7afd21b0bc93860d66e6ccdc96ccf1b039ea31871119a3deaa271352c3`). W repozytorium NIE ma tego oryginalnego pliku ani jego źródeł. Ten SHA identyfikuje jedynie lokalny załącznik; brak bajtów w repo blokuje dokładny Windows build i dostarczenie paczki przez updater.
- Import Wow.exe -> `EpochConnection.dll` ordinal #1, oryginalna DLL eksportuje ordinal #1 `EpochConnectionAnchor`, #2 `EpochConnectionCheck`, #3 `EpochConnectionStatus`. Nie zmieniać eksportów, ABI ani kodu tunelu. Lokalny plik miał 8 sekcji, rozmiar nagłówków `0x400`, koniec istniejącej tablicy sekcji `0x340`, wolne 40 bajtów pod nowy opis, pierwszy raw section `0x400`. Brak deklarowanego Security directory, istnieją TLS oraz base relocations.
- `tools/epoch_connection_loader_patch.py` to **oddzielny, dokładnie przypięty prototyp kopii binarki**. Dokleja wyłącznie nową tablicę importów w nowej sekcji, zachowuje sekcje, eksporty, dotychczasowe importy, TLS, base relocations i debug; importuje niezależny `Wow335Loader.dll` ordinal #1. Oryginału nie nadpisuje. Nie tworzy gotowego klienta i nie instaluje niczego w katalogu gry.
- `src/Loader12340/Wow335Loader.c` jest osobnym kodem źródłowym loadera odczytującego `dlls.txt`. Wcześniej budowa MSVC x86 DLL i eksport ordinal #1 przeszły. Nie ma dowodu, że sekwencja startowa przez zmodyfikowaną zależność sieciową jest zgodna z klientem i serwerem.

## Bramka dostarczenia
Nie wydawać ani nie wpisywać do `runtime/current.json` nowego EpochConnection.dll przed przetestowaniem dokładnych bajtów źródłowych, poprawności eksportów i importów przez **Windows PE32 x86 CI**, obecności prawidłowego loadera, kompletnego SHA256 manifestu, automatycznego backup/rollback w updaterze oraz pozytywnego testu gry/logowania na konkretnym SHA. `main`, `work`, canonical `Wow.exe`, oryginalny `EpochConnection.dll` i aktualny updater bez zmian. CI syntetycznej próbki PE32 nie jest testem sieci ani gry. Nie udostępniać osobnej paczki instalacyjnej poza updaterem.
