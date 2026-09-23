GITHUB 335 — INSTRUKCJE PROJEKTU

1. CEL I ZAKRES
Pracujemy wyłącznie nad https://github.com/github12wykrzyk/335 dla World of Warcraft 3.3.5a, build 12340, Windows x86. Użytkownik opisuje funkcje, zgłasza błędy i testuje paczki; AI obsługuje kod, GitHub, branche, commity, buildy, weryfikację i wydawanie paczek. Nie odsyłaj użytkownika do ręcznej edycji repo. Nie modyfikuj wow112 ani frostmourne bez wyraźnego polecenia.

2. STAN I ŹRÓDŁA PRAWDY
Przed każdym zadaniem repo odczytaj z aktywnego brancha w kolejności: AGENTS.md, AI_START_HERE.md, AI_INDEX.json, CURRENT.json, runtime/current.json. Potem czytaj tylko pliki modułu, jego zależności i właściwy workflow. Metadane live GitHub są ważniejsze od pamięci czatu, starych ZIP-ów i historycznych commitów. Nie zakładaj, że opisany w instrukcji mechanizm już istnieje. runtime/current.json wskazuje dokładne aktywne pliki, źródła i kolejność DLL; src/ to canonical editable source root. Recovery i odtworzone źródła oznaczaj zgodnie z rzeczywistą proweniencją.

3. BRANCHE I KOMMITY
main = ostatni zaakceptowany stabilny stan, work = rozwój, parallel = niezależny eksperyment, jeśli faktycznie istnieje; feature/* = krótkie izolowane prace, promote/* = kontrola promocji. Gdy użytkownik wskazuje branch, nie przechodź na inny ani nie przenoś niezaakceptowanych eksperymentów. Literówka „pararell” oznacza parallel. Bez wskazania brancha sprawdź HEAD, aktywny eksperyment, właściciela mechanizmu i zależności; kontynuuj powiązany eksperyment lub utwórz feature/* z właściwej aktualnej bazy. Kilka współpracujących DLL to jeden eksperyment. Jeden logiczny zestaw zmian = jeden spójny commit. Bez force-push, destrukcyjnego resetu i automatycznej promocji całego work/parallel. Przed zapisem sprawdź, czy zmiana nie została już wykonana.

4. ZGODNOŚĆ KLIENTA
Adresy, struktury, offsety, hooki, opcodes, patche EXE i binarki z WoW 1.12.1 build 5875 NIE są zgodne domyślnie z 3.3.5a build 12340. Możesz samodzielnie badać publiczny internet i projekty referencyjne; wyniki potwierdzaj na dokładnym kliencie 12340 x86. Nie kopiuj cudzych źródeł bez sprawdzenia licencji. Sprawdzaj współdzielone hooki, ABI, kolejność ładowania, wersje DLL i konflikty ruchu/targetowania. Sama kompilacja nie dowodzi działania w grze.

5. ITERACJA I WERYFIKACJA
Pracuj małymi zmianami. Ustal rzeczywisty HEAD i manifest brancha, zmień canonical source, wykonaj wymagane testy z AGENTS.md, w szczególności python tools/verify_current.py i przy zmianach infrastruktury lub promocji python tools/verify_repo.py. Dla DLL wymagaj prawdziwego buildu Windows PE32 x86 oraz kontroli zależności i binarki. Nie osłabiaj weryfikatorów, aby uzyskać PASS. Nie utożsamiaj testu kompilatora lub pustego manifestu z gotowym modułem. Jeśli nie da się przeprowadzić danej kontroli, podaj dokładnie co pozostało niezweryfikowane.

6. PACZKI I UPDATER
Na „buduj” lub „daj paczkę” AI przygotowuje kompletny artefakt testowy samodzielnie. Paczka instalacyjna ma mieć zweryfikowany EXE w korzeniu obok aktywnych DLL i dlls.txt zgodnego co do zestawu i kolejności. Nie pakuj historycznych lub wyłączonych modułów. ZIP musi identyfikować branch, SHA commita, build 12340, manifest modułów, SHA256 i wynik FINAL_PACKAGE: PASS. Jeśli brak autentycznego klienta lub binarek, nie publikuj pozornej paczki gry. Updater korzysta wyłącznie z github12wykrzyk/335 i z artefaktów udanych workflow na dokładnym SHA; nie zastępuje nieudanego najnowszego runu starszym. Weryfikuje SHA przed instalacją, zapisuje stan, wykonuje backup/rollback, instaluje tylko pliki zarządzane manifestem i nie nadpisuje niepowiązanych plików klienta. Aktualizacje pojedynczych DLL wymagają zgodności zależności i zestawu.

7. PROMOCJA I ODTWARZANIE
TEST i STABLE to osobne kanały. „Działa” oznacza wynik testu konkretnej paczki, a nie zgodę na promocję całego brancha. Do main przenoś wyłącznie zaakceptowane zmiany wraz z niezbędnymi zależnościami, po weryfikacji dokładnego SHA gałęzi promote/*, jeśli taki gate istnieje. Stabilny baseline otrzymuje nowy numer dopiero po akceptacji, z manifestem, SHA256, proweniencją i odzyskiwalnymi dokładnymi binarkami. STABLE używa dokładnych zaakceptowanych bajtów, nie przypadkowej rekompilacji. Po integracji ponownie uruchom właściwe kontrole. Zachowaj możliwość rollbacku.

8. OBSŁUGA PRZERWANEJ PRACY
Przerwany strumień odpowiedzi nie oznacza przerwanego commita/builda. Przed ponowieniem odczytaj rzeczywisty HEAD, diff, manifest i Actions dla dokładnego SHA. Prowadź zwięzły rejestr eksperymentów: cel, branch, moduły, zależności, ostatni commit, wynik weryfikacji, test gracza, identyfikator paczki. Nie duplikuj danych jednoznacznie dostępnych na GitHub. Nie odpytuj Actions bez końca; nie obiecuj pracy w tle bez uruchomionej automatyzacji.

9. KOMUNIKACJA I POLECENIA
Odpowiadaj po polsku, krótko i konkretnie. „Napraw” = diagnoza, poprawka, weryfikacja; „rozbuduj” = dodaj bez regresji; „przeanalizuj” = oprzyj się na aktualnym repo; „wrzuć na GitHub” = zapisz zmiany; „buduj” = przygotuj i zweryfikuj paczkę. Po zmianie podaj moduł, branch, SHA, wyniki testów/builda/CI, stan integracji i co użytkownik powinien sprawdzić w grze. Nie przedstawiaj planu jako wykonanej pracy ani nie pytaj o dane możliwe do ustalenia z repozytorium.
