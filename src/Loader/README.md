# Uniwersalny loader modułów WoW 3.3.5a / 12340 x86

`src/Loader/loader_win32.c` jest zewnętrznym procesem x86 wbudowanym w updater.
Nie jest to DLL wstrzykiwana do Wow.exe przez updater i nie wymaga patchowania EXE.
Uprawnia do uruchomienia tylko zweryfikowany instalator TEST/STABLE i lokalny
`dlls.txt` utworzony z aktywnego `runtime/current.json`.

1. Updater weryfikuje SHA256 dokładnie wybranego Wow.exe, SHA256 dlls.txt,
   identyczność zbioru DLL z `installed.json`, SHA256 **każdej** DLL i nagłówek PE32 x86.
   Brak lub dodatkowa DLL, uszkodzony hash bądź modyfikacja listy blokują start.
2. Loader czyta maksymalnie 32 unikalne lokalne nazwy DLL w kolejności z
   `dlls.txt`. Odrzuca ścieżki, duplikaty, nazwy z `..`, pustą listę,
   przekroczenie limitu i brak plików. Przed uruchomieniem Wow.exe
   wszystkie biblioteki muszą zostać załadowane i eksportować ABI poniżej.
3. Po utworzeniu procesu gry loader ustala wątek widocznego okna i instaluje
   `WH_GETMESSAGE` oraz `WH_CALLWNDPROC` osobno dla każdego modułu
   w kolejności manifestu. Zainicjalizowane moduły dostają wiadomości na
   wątku gry. Loader nie wywołuje funkcji klienta z zewnętrznego procesu.
4. Moduł odbiera 1 = enable, 2 = tick, 0 = disable pod identyfikatorem
   zwróconym przez `W335_MessageId`. Callbacki muszą używać
   `CallNextHookEx` i NIE mogą instalować niezależnych konfliktujących hooków
   na współdzielonych adresach gry. Przy błędzie podczas instalacji hooków
   loader wyłącza załadowane moduły, odpina hooki i kończy uruchomiony przez
   siebie proces WoW (fail closed). Sam exit code 0 nie jest testem w grze.

## ABI nowej DLL

Eksporty C / WinAPI x86 wymagane do uruchomienia nowego modułu:

```c
__declspec(dllexport) UINT WINAPI W335_MessageId(void);
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l);
__declspec(dllexport) LRESULT CALLBACK W335_CallWndProc(int code, WPARAM w, LPARAM l);
```

Dopuszczane są nazwy eksportów x86 bez dekoracji lub odpowiedniki stdcall
_W335_MessageId@0`, _W335_HookProc@12`, _W335_CallWndProc@12`.
W przypadku **wyłącznie** `AutoLoot335.dll` loader obsługuje legacy
`AL335_*`, aby pozostawić aktualnie zarejestrowaną binarkę bez zmian.

Każdy przyszły moduł wymaga kompletnego wpisu `runtime/module_registry.json`
(źródła, zasoby, zależności, właściciel i build x86) oraz zgodnego
`runtime/current.json`, rzeczywiście zbudowanej DLL, bramek CI
i weryfikacji w grze. Loader nie pobiera ani nie uruchamia dowolnych DLL
znalezionych na GitHubie i nie obchodzi ochrony kolizji zasobów. Dodanie DLL
zgodnej z ABI do poprawnego manifestu NIE wymaga ponownej kompilacji loadera.
