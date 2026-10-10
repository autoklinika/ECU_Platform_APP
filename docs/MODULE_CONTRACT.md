# Instalowalne aplikacje i kontrakt modułów

Status: **projekt semantyczny SDK; brak implementacji loadera i publicznego ABI**.
Nie jest to obietnica, że obecny build przyjmie opisane pakiety.

## 1. Jednostka rozszerzenia

Moduł ma stabilne ID, wersję i własny zakres odpowiedzialności. Host zna jego
manifest, operacje i wymagania, a nie kod OEM. Dodanie DUT korzystającego z już
obsługiwanych usług nie wymaga edycji pozostałych DUT, CORE ani menu głównego.

Rozróżniamy aplikację DUT, funkcję (np. analizę/raport) i adapter sprzętowy.
Są pakowane i testowane jako oddzielne odpowiedzialności; adapter nie staje się
jednak zwykłym niezaufanym dodatkiem z prawem do urządzenia.

## 2. Przykładowa zawartość pakietu

Poniższa struktura jest kandydatem, nie zatwierdzonym formatem archiwum:

```text
org.example.dut.app/
  manifest.json
  profiles/
  operations/
  resources/
  ui/
  binaries/           # tylko kiedy potrzebny jest własny kod
  tests/              # źródłowe wektory i testy kontraktowe
```

Nazwa rozszerzenia `.ecupkg`, format podpisu, schemat JSON i dokładne nazwy pól
będą ustalone podczas pierwszego działającego pakietu. Testów dostarczonych przez
pakiet nie wykonuje się automatycznie z uprawnieniami instalatora.

Manifest powinien opisywać: ID i wersję; zgodne API platformy; zależności;
capability wymagane i opcjonalne; operacje i ich ryzyko; wymagany sprzęt;
obsługiwane OS/architektury; wariant wykonania; zasoby widoków; pochodzenie oraz
integralność zawartości. Nie zawiera haseł i nie nadaje sobie uprawnień.

## 3. Wymagania, uprawnienia i dostępność to nie to samo

Żądanie capability przez aplikację nie jest przydziałem. Host sprawdza kolejno:
zgodność kontraktu, obecność implementacji, obecność sprawnego sprzętu,
politykę uprawnień i dostępność zasobu w bieżącej sesji.

Aplikacja wymagająca pomiaru prądu nie może otrzymać sztucznego zera, gdy brak
miernika. Brak capability wymaganej blokuje uruchomienie odpowiedniej funkcji.
Brak opcjonalnej pozwala na jawnie ograniczony zakres, bez wyniku udającego
pełny test. Nie stosujemy automatycznego fallbacku zmieniającego semantykę.

## 4. Trzy klasy wykonania

### A. Dane i ograniczone procedury na gotowych usługach

Parametry, adresowanie, warianty ECU, dekodery opisowe, widoki i wywołania
zatwierdzonych operacji. Format musi mieć walidację zakresów, rozmiarów, czasu
oraz dozwolonych działań. To nie jest dowolny program ukryty w JSON.
Dane OEM mogą obejmować ramki, ale nie są publiczną możliwością wysyłania
arbitralnych ramek przez GUI. Aktywuje je polityka zaufanego runtime.

### B. Własna logika dodatku poza torem krytycznym

Własny kod C++ lub inny zaakceptowany runtime w kontrolowanym procesie roboczym.
Dostaje wersjonowany kontrakt IPC i ograniczone capability, bez SocketCAN,
sudo, dowolnych plików hosta i prywatnych struktur CORE. OS musi wymuszać te
granice; nazwanie procesu workerem ich nie tworzy.

Timeout, nadmierny ruch IPC, niepoprawny komunikat, przekroczenie pamięci albo
awaria mają jawny wynik. Nadzór nad aktywnym DUT nie zależy od tego procesu.
Proces nie staje się publiczną usługą sieciową ani demonem instalowanym osobno
ręcznie przez operatora. Uruchamianie na żądanie jest modelem docelowym.

### C. Zaufane rozszerzenie czasu krytycznego lub sprzętu

Nowy mechanizm wymagający pracy w krytycznej ścieżce, z niedostępnymi dotąd
kontraktami czasowymi, transportowymi lub bezpieczeństwa, jest rozszerzeniem
platformy. Wymaga review, testów przenośności i terminowości oraz świadomej
aktywacji. Natywna biblioteka wewnątrz hosta nie jest odizolowana od jego pamięci.

Nie wymuszamy od razu dynamicznych `.so`/`.dll`. Sposób dostarczenia zaufanego
rozszerzenia wynika z ograniczeń ABI, narzędzi i testów. Przyszła instalacja może
wymagać aktualizacji runtime. Nie budujemy IPC z deadline'em równym okresowi
każdej ramki jako obejścia braku możliwości runtime.

## 5. API modułu

Publiczny kontrakt powinien objąć opis możliwości, przygotowanie, aktywację,
przyjmowanie nazwanych operacji, migawki wyników, żądanie zakończenia i jawny
wynik błędu. Są to odpowiedzialności, nie obecne klasy ani nazwy metod.

Komendy mają ID, sesję/generację, ograniczenia oraz termin ważności tam, gdzie
ma on znaczenie. Moduł nie przechowuje drugiej prawdy o sesji i nie odczytuje
cache'u GUI. Rozłączenie przeglądarki nie jest destruktorem modułu.

Usługi diagnostyczne, cykliczne, pomiarowe i wynikowe są składane. Jeden moduł
może jednocześnie korzystać z UDS i własnego CAN. Moduł raportowania korzysta
z kontraktu wyników, a nie z klas SAC czy kodu ekranu EGR.

CORE C++ udostępnia kontrakty źródłowe, nie stabilne ABI dla dowolnego kompilatora.
Granica procesu wymaga jawnej serializacji; nie wysyłamy surowego układu struktur
C++ ani wskaźników. Natywne komponenty muszą deklarować wspieraną platformę i ABI.

## 6. Instalacja i aktywacja

Docelowy przebieg: przyjęcie do stagingu, walidacja zawartości i pochodzenia,
rozwiązanie zależności, sprawdzenie zgodności i polityki, rejestracja pakietu,
a następnie aktywacja we właściwym momencie.

Instalacja nie nadaje CAN i nie uruchamia samoczynnie kodu z pakietu. Nie może
rozpakować plików poza własny katalog, zastąpić hosta przez traversal/symlink ani
wykonać arbitralnego hooka z prawami administratora. Weryfikacja podpisu nie
zastępuje testów, zgody ani kontroli ograniczeń.

Rejestr instalacji nie jest zamrożoną rejestracją aktywnego CORE. Aktualizacja
aktywnego modułu czeka na bezpieczny koniec sesji. Połączenia callbacków nie są
usuwane w locie. Nowy graf powstaje przed rozpoczęciem pracy kolejnej sesji.

Aktualizacja ma być transakcyjna: kompletna wersja albo brak zmiany. Ostatnie
znane wydanie pozostaje dostępne do rollbacku. Zmiana schematu trwałych danych
wymaga migracji z kopią; sam rollback plików programu nie cofa bazy danych.

## 7. Zależności i zgodność

Pakiet deklaruje wspieraną wersję API i zależności. Konflikt nie może po cichu
podmienić usługi wymaganej przez SAC. Współdzielone usługi platformy mają jawnie
kontrolowaną wersję; prywatne biblioteki dodatku pozostają w jego zakresie.
Nie obiecujemy współistnienia dowolnych niezgodnych wersji tego samego runtime.

Brak wymaganej możliwości kończy się czytelnym komunikatem: czego brakuje,
jakiej wersji/sprzętu potrzeba i co zostało zablokowane. Instalator nie włącza
automatycznie nowej funkcji ryzykownej i nie aktualizuje współdzielonego CORE
w czasie pracy DUT.

Wsteczna zgodność dotyczy semantyki: wyniku operacji, anulowania, świeżości,
bezpieczeństwa i jednostek, nie tylko identycznej nazwy metody.
Rozwój SDK opisuje [CORE_EVOLUTION.md](CORE_EVOLUTION.md).

## 8. Modułowe WebGUI

Powłoka odpowiada za katalog, nawigację, ustawienia i standardowe komponenty.
Moduł opisuje własne widoki, parametry, DTC, wykresy i operacje. Początkowo
preferujemy kontrolowane komponenty i widoki deklaratywne; własny JavaScript
wymaga odrębnej polityki izolacji i dystrybucji.

Nie generujemy osobnego frontendu z ukrytym dostępem do sprzętu. To samo API
odrzuca niedozwoloną operację niezależnie od tego, czy przycisk jest widoczny.
Pakiet UI i część backendowa mają zgodną wersję i manifest; nie mieszamy wydań.

## 9. Minimalny dowód kontraktu

Pierwszy pakiet testowy bez sprzętu ma dać się zarejestrować, ocenić pod kątem
zgodności, aktywować i wyłączyć bez zmian hosta. Negatywne przypadki: brak wymaganej
usługi, duplikat ID, niezgodne API, nieuprawniona operacja i aktualizacja aktywnego
modułu. Następnie kontrakt sprawdzamy na SAC, aktuatorze i raporcie.
Nie projektujemy kompletnego marketplace'u przed wykonaniem tych dowodów.
