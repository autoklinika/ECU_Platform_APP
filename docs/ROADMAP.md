# Plan implementacji

Plan nie nadaje zgody na wdrożenie, nadawanie CAN ani scalenie `main`.
Kolejne zadanie ma kończyć się sprawdzalnym działaniem, nie tylko nowymi katalogami.

## 0. Samodzielny fundament — bieżący zakres

Import byte-exact CORE V2 i wszystkich jego 20 zestawów testów, nowy minimalny
build, kontrola pochodzenia, kontrola granic i nowa dokumentacja z rozmowy.

**Akceptacja:** kopia kompletna; brak zależności od starych repo; build i CTest
w zadeklarowanych środowiskach; bez dostępu do sprzętu. Szczegółowy wynik w
[VALIDATION.md](VALIDATION.md). To nie jest jeszcze działający host aplikacji.

## 1. Najmniejszy działający host i pakiet bez sprzętu

Niewielki, przenośny program C++; minimalny manifest i kontrakt modułu sprawdzane
na neutralnej aplikacji testowej. Katalog, zgodność, aktywacja, operacja z wynikiem,
wyłączenie. Nie implementujemy marketplace'u, sandboxa wszystkich języków ani
własnego uniwersalnego silnika skryptowego.

**Akceptacja:** dodanie/wyłączenie pakietu bez edycji hosta; niezgodne API, brak
capability, duplikat ID i nieuprawniona operacja odrzucone. Host pracuje bez GUI.
Błąd aplikacji testowej ma kontrolowany wynik. Linux i Windows w CI.

## 2. Zaufany runtime i granica sprzętu

Połączenie hosta z kontraktami CORE: jeden executor aktywnych obiektów,
kompozycja sesji, kontrolowane zakończenie, migawki i błędy. Atrapa CAN do testów
oraz nowy adapter platformowy, bez kopiowania starej infrastruktury procesowej.
Nie tworzymy drugiego ogólnego CanBusRuntime, schedulera ani ResourceManagera.

**Akceptacja:** testy własności kanału, limitów, czasu życia, timeoutu i cleanupu;
GUI ani worker dodatku nie może otworzyć urządzenia poza hostem. Weryfikacja
adaptera fizycznego jest oddzielnym, uzgodnionym testem, nie efektem instalacji.

## 3. SAC — pierwsza pełna ścieżka użytkowa

Moduł SAC na publicznym kontrakcie: kontrolowana procedura 250/500 kbit/s,
identyfikacja VIN/SW/HW, trwała sesja, parametry oraz odczyt DTC. Zachowujemy
wiedzę o zweryfikowanym protokole; implementacja modułu jest nowa.

Cienkie API i minimalne WebGUI pojawiają się na tej działającej ścieżce, nie
jako osobny wielomiesięczny projekt. Wybranie DUT i ekran identyfikacji zachowują
prostotę V1. Kolejne ekrany nie zmieniają własności CAN i sesji.

**Akceptacja:** odczyty w tej samej sesji, brak powtarzania identyfikacji przy
nawigacji, status świeżości i jakości, dane powiązane z sesją; backend działa bez
GUI; testy Chromium z atrapą API; po uzgodnieniu testy fizyczne obu wariantów
SAC, wyłącznie gdy rzeczywiście dostępne. Brak automatycznego kasowania DTC.

## 4. Drugi rodzaj DUT — EGR / VGT

Sprawdzenie, czy kontrakt mieści własne ramki, cykliczne sterowanie oraz pełną
maszynę stanów testu bez zależności od GUI. Pierwszy kandydat: MAN Sonceboz EGR;
nie uruchamiamy go tylko dlatego, że był kiedyś podłączony.

**Akceptacja:** dowód w symulacji; brak zmian w kodzie SAC; brak nowego właściciela
CAN; limity i safe-stop; dopiero po autoryzacji pomiary fizyczne. Obciążenie GUI,
zoom, raportowanie i utrata klienta nie wyznaczają ruchu ani czasu pomiarów.
Mierzymy nie tylko TX, ale RX, rampę, feedback i kroki testu.

## 5. Niezależna funkcja i lifecycle pakietów

Raport z wyników sesji bez zależności od klasy konkretnego ECU. Na realnych
potrzebach implementujemy izolowany worker, zasady IPC i uprawnień, jeżeli są
potrzebne. Walidacja pakietu, zależności, transakcyjna aktualizacja, wyłączenie
oraz rollback. Precyzujemy stabilne SDK po sprawdzeniu trzech rodzajów modułów.

**Akceptacja:** awaria raportu nie przerywa zdrowej sesji; brak dostępu workera
do CAN; błędny pakiet nie zmienia działających modułów; aktywna sesja blokuje
niebezpieczną aktualizację; odtworzenie poprzedniej wersji działa także dla
migrowanych danych albo jawnie wymaga ich kopii.

## 6. Wydanie na stanowisko

Jedna odtwarzalna procedura instalacji całego wydania, ograniczone uprawnienia
kiosku i backendu, identyfikowalny build, test start/stop/awaria i rollback.
Wyłączenie usług V2/V3 następuje wyłącznie po decyzji właściciela. Zmiany
sprzętowe, Power Manager i ryzykowne operacje dostają własne kryteria.

**Akceptacja:** przegląd konkretnego wydania, zgodność z dostępnym sprzętem,
regresja modułów, decyzja o produkcyjnym `main`. Zielone CI nie jest zgodą na
wdrożenie ani na fizyczne działanie.

## Rzeczy celowo nieprzesądzone

Format podpisanego pakietu, szczegółowy wire schema SDK, biblioteka HTTP,
framework WebGUI, sandbox workerów, model ABI natywnych rozszerzeń, dokładny
rozkład wątków i priorytetów oraz trwała baza wyników. Wybieramy je na podstawie
wymagań i dowodu działania, bez mnożenia alternatyw równolegle.
