# Zasady projektu

Status: obowiązujące ustalenia właściciela. Data bazowa: 2026-10-10.

## 1. Cel i samodzielność

ECU Platform APP jest narzędziem laboratoryjnym do elektroniki TRUCK/AGRI/OHV.
Nie sprowadzamy DUT do ECU wymagającego UDS. Aktuatory, sensory i inne moduły
korzystające z własnych ramek są równorzędnymi przypadkami użycia.
Domyślne stanowisko obsługuje jeden DUT; nie budujemy mikroserwisów dla
hipotetycznej floty stanowisk. Wymienne peryferia i multiplatformowość pozostają
wymaganiami podstawowymi, a nie późniejszym portem.

Repozytorium nie zależy przy buildzie od V1, V2, V3 ani AI Platform.
Firmware Power Managera pozostaje osobnym projektem; tutaj powstanie wyłącznie
odpowiedni adapter i klient usług sprzętowych.

## 2. Własność decyzji i gałęzi

Właściciel projektu decyduje o zakresie produktu, zmianach architektury i wydaniu.
Zgoda na kodowanie, dokumentację, testy lub wysłanie gałęzi nie oznacza zgody na
zapis, fast-forward, rebase, squash ani merge do produkcyjnego `main`.
Przed scaleniem przedstawiamy konkretny commit/diff i wyniki.

Nowe repozytorium było całkowicie puste. Pierwszy import powstaje na
`foundation/core-v2-architecture-20261010`. Nie tworzymy pustego ani wypełnionego
`main` w celu obejścia wymagania zgody. Przy braku gałęzi bazowej nie da się
uczciwie otworzyć PR do `main`; pierwsze ustanowienie bazowej gałęzi jest osobną
decyzją właściciela. Automatyczne wskazanie pierwszej gałęzi jako domyślnej przez
GitHub nie jest zgodą na wdrożenie produkcyjne.

## 3. Sprzęt i operacje

Testy domyślnie korzystają z atrap i danych syntetycznych. Instalacja, kompilacja
oraz CI nie nadają CAN, nie zmieniają jego bitrate, nie sterują przekaźnikami i
nie uruchamiają DUT. Do weryfikacji fizycznej potrzebny jest określony zakres,
aktualny stan stanowiska i upoważnienie właściciela.

Kasowanie DTC, sterowanie EGR/VGT, zasilanie, zapisy i flashowanie wymagają
osobnej jawnej autoryzacji. Zezwolenie historyczne nie przechodzi na nowy projekt.
Nie wystawiamy generycznego endpointu shell/UDS/CAN jako obejścia brakującej funkcji.
Nie przechowujemy w publicznym repo haseł, kluczy, tokenów, VIN-ów ani surowych
prywatnych śladów CAN. Testowe identyfikatory muszą być syntetyczne.

## 4. Sposób rozwoju

Małe, kompletne zadania zamiast mnożenia etapów i skryptów wdrożeniowych.
Najpierw minimalny kontrakt i działający dowód; następnie rozwój przez rzeczywiste
przypadki SAC, aktuatora oraz funkcji bez dostępu do sprzętu.

Nie zmieniamy CORE dla wygody profilu. Nie blokujemy uzasadnionej ewolucji CORE
w nieskończoność. Decyzję poprzedza analiza odpowiedzialności i ograniczenia
kontraktu, a kończy pełna regresja oraz świadoma polityka zgodności.

Wyniki testów opisujemy precyzyjnie. Brak kompilacji Windows lub testu sprzętowego
pozostaje brakiem weryfikacji, nawet gdy testy Linux są zielone.
Zgodność normatywna, certyfikacja oraz twardy czas rzeczywisty wymagają osobnych
dowodów; sam audyt kodu lub CI ich nie ustanawia.
