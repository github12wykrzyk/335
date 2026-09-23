# Loader 12340 — etap 1: analiza binarna (TEST)

Baza: branch `work`, SHA `18905f8c9d3ebc57588e12b1f2f76416572c2659`.
Target: user-selected `Wow.exe` 3.3.5.12340 x86, SHA256 `2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d`.

Wzorzec 1.12: przesłany `WoW.exe` (SHA256 `97ea82ab7a82ed88bc5a155ab9bbf7e0bcdb5b9f892a47c6f8edabe1496bfc75`) zawiera łańcuchy `twloader.dll` i `func1`, lecz w statycznej tablicy importów nie ma wpisu `twloader.dll`. To rozróżnienie jest ważne: same bajty nie dowodzą sposobu uruchomienia loadera. Przesłany `twloader.dll` (SHA256 `fc4adbde2d4e1aaf74f9403a59f8663dcf00b45fed22cd858d12258a17c42bea`) zawiera `dlls.txt` i `LoadLibraryW`; nie jest źródłem offsetów 12340.

Plik `tools/audit_loader_12340.py` audytuje **wyłącznie** importy PE32 i znaczniki tekstowe dokładnego przypiętego klienta. Wywołanie: `python tools/audit_loader_12340.py --exe Wow.exe --report dist/loader_12340_audit.json`. Nie modyfikuje, nie uruchamia ani nie ładuje DLL. Raport statyczny nie stanowi dowodu działającego loadera.

Następne bramki: zatwierdzić odczyt faktycznego import table 12340 na dokładnym SHA, ustalić zgodny model uruchomienia modułów, zweryfikować wersję PE32 x86 i zależności każdego modułu z rejestru; dopiero potem rozważyć zintegrowany build TEST. W `runtime/current.json` nie ma aktualnie aktywnych DLL, więc `FINAL_PACKAGE: PASS` i test gry nie zostały uzyskane. `main`, `Wow.exe` i updater pozostają nietknięte.
