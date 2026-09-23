# WoW335Updater 0.3.13-335 — jeden launcher dla wszystkich branchy

Jedyny updater Windows x86 jest budowany i publikowany na `work`; self-update
pobiera updater aktualnego HEAD `work` niezależnie od wyboru brancha gry.
Wybrany branch gry dostarcza tylko kompletną paczkę runtime z własnego
aktualnego HEAD i `FINAL_PACKAGE: PASS`. TEST = work, STABLE = main;
feature/* wymaga własnej zweryfikowanej paczki.

Jedyny launcher zarejestrowanych DLL: `src/Loader/loader_win32.c`, budowany
jako rzeczywisty Windows PE32 x86 `WoW335RuntimeLoader.exe` i osadzany
w `WoW335Updater.exe` jako `WoW335Runtime.Loader.exe`. Updater sprawdza
dokładny SHA256 `Wow.exe`, listy `dlls.txt`, wszystkich zarejestrowanych
DLL i ich PE32 x86. `dlls.txt` jest generowany z manifestu paczki, a
nie z przypadkowych plików z dysku albo GitHuba. Moduły deklarują zasoby,
zależności, źródła i build x86 w `runtime/module_registry.json` i
eksportują ABI `W335_*`; jedynie już zarejestrowany AutoLoot może użyć
starych eksportów `AL335_*`. Dodanie kompatybilnej DLL nie wymaga
rekompilacji launchera, lecz wymaga rzeczywistego buildu i weryfikacji
całej paczki oraz testu w grze.

Odrębny eksperymentalny launcher „Natywny AutoLoot TEST” został usunięty
z kodu i GUI kanonicznego updatera. Dodatek diagnostyczny AutoLoot jest
tylko dodatkiem Lua, nie launcherem DLL.
`EpochConnection.dll` pozostaje oryginalną zależnością `Wow.exe` i
nie może być kasowana. Przy wykryciu starego zarządzanego loadera Epoch
updater blokuje instalację i uruchomienie modułów; migracja musi najpierw
przywrócić z weryfikowanego backupu oryginalną DLL, bez podwójnego hooka.
Nie usuwaj obcych plików użytkownika ani nie pomijaj tego zabezpieczenia.

Updater instaluje wyłącznie dokładny kompletny zestaw, zachowuje backupy
zarządzanych plików i umożliwia rollback, Sprawdź / napraw, Aktualizuj
i uruchom oraz raporty po podglądzie. Token repo wymaga Contents: Read i
Actions: Read; osobny token raportów Issues: Read and write. Tokenów nie
umieszczać w repo, nie zmieniać `Data/<locale>/realmlist.wtf`.
Build updatera i CI nie dowodzą działania w grze: do promocji STABLE
potrzebny jest test konkretnego SHA na WoW 3.3.5a build 12340 Windows x86.

## 0.3.14-335 — kompaktowy dashboard

Trzy przyciski codziennej obsługi: Sprawdź, Aktualizuj i Aktualizuj i uruchom.
Wszystkie dotychczasowe operacje pomocnicze, w tym bezpośredni start gry,
self-update updatera, weryfikacja/naprawa, raporty i rollback, pozostają
w zwijanym panelu w tym samym oknie. Self-update jest nadal oddzielną
operacją z kontrolowanym restartem — nie deklarujemy automatycznego
wznowienia aktualizacji gry po restarcie updatera. Ustawienia zapisują się
po opuszczeniu pola lub zmianie brancha; token DPAPI pozostaje chroniony.
Widoczna linia DLL pochodzi z lokalnego manifestu zainstalowanej paczki,
nie z eksperymentów GitHub; status CI nie jest testem w grze.

## 0.3.15-335 — wszystkie branche w jednym rzędzie

Pełnoszeroka, pojedyncza belka nad konfiguracją pokazuje wszystkie
odkryte branche. Sześć obecnych branchy mieści się w jednym rzędzie
z małymi ramkami; nowe branche nie są ukrywane ani przenoszone do
kolejnego wiersza. Gdy zabraknie miejsca, tylko belka przewija się
poziomo. Lista jest pobierana stronicowo z GitHuba (do 10000 pozycji
z jawnym błędem przy przekroczeniu), odświeża status każdego HEAD.
Szczegóły SHA i workflow pozostają w podpowiedziach oraz monitorze.
Layout nie przebudowuje się podczas samych zmian statusu, więc nie
gubi pozycji paska. Test WinForms x86 sprawdza jeden rząd oraz to, że
dodanie dziewięciu branchy nie pomija ani nie zawija statusów.
