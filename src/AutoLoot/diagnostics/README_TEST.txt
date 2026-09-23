WOW 3.3.5a / 12340 — AutoLoot: oddzielny test diagnostyczny 0.1.0
Repo: github12wykrzyk/335 ; branch: feature/autoloot-12340

NIE JEST TO GOTOWA PACZKA GRY ANI DZIALAJACA DLL. Nie instaluj
AutoLoot335_Adapter_DIAGNOSTIC.dll. Ten dodatek Lua testuje TYLKO reakcje
klienta na OTWARTE RECZNIE okno lootu i zlecanie zbierania slotow.
Nie wyszukuje samodzielnie zwlok, nie wykonuje ruchu ani spoofingu,
nie integruje sie z updaterem i nie naprawia komunikatu EXE_ONLY.

1. W updaterze WoW335 v0.3.4-335 z eksperymentalnego buildu
   wybierz katalog gry i kliknij Instaluj test AutoLoot. Alternatywnie:
   rozpakuj folder WoW335AutoLootDiag do katalogu gry
   World of Warcraft/Interface/AddOns/WoW335AutoLootDiag/
   (pliki .toc i .lua musza byc bezposrednio w tym folderze).
2. Uruchom WoW.exe, w ekranie wyboru postaci: AddOns -> wlacz
   WoW335 AutoLoot Diagnostic; w razie potrzeby zaznacz Load out of date.
3. W grze wpisz /al335 status (domyslnie OFF).
4. Znajdz zwykle martwe NPC z prawem do lootu, wpisz /al335 on.
   OTWORZ zwloki RECZNIE prawym klikiem; addon podejmie max 3 proby
   zebrania dostepnych slotow w tym oknie. Testuj tez pelne torby.
5. Wpisz /al335 off, /al335 log; wyslij zrzut czatu i informacje
   czy przedmioty i zloto trafily do toreb. /reload zapisze raport do
   WTF/Account/<konto>/SavedVariables/WoW335AutoLootDiag.lua
   (mozesz tez zalaczyc ten plik, bez danych logowania).

UWAGA: po /al335 on test obejmuje dowolne recznie otwarte okno lootu,
w tym skrzynie i mining. W trakcie testu nie otwieraj innych obiektow.
W razie zachowania niepozadanego /al335 off.

Wynik tego testu nie dowodzi dzialania natywnego AutoLoot. Ostateczna
paczka WoW335 wymaga zintegrowanej, zweryfikowanej DLL PE32 x86 i
FINAL_PACKAGE: PASS dla dokładnego SHA oraz zestawu DLL.

6. W nowym updaterze kliknij Wyslij log AutoLoot. Zostana wczytane
   tylko linie zdarzen z zapisanych SavedVariables; zaakceptuj podglad
   przed wyslaniem do GitHub Issues. Oddzielny token raportu musi miec
   uprawnienia Issues: Read and write. Nie musisz wysylac surowych plikow
   WTF ani danych konta. Dziala niezaleznie od pustego manifestu DLL.
