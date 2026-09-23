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
