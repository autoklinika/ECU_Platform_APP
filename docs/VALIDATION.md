# Weryfikacja fundamentu

Data: 2026-10-10. Dotyczy nowego repozytorium APP, nie dawnego wydania V2.

## Wykonano na CM5

Środowisko: Linux aarch64, GCC 14.2.0, CMake 3.31.6, Ninja 1.12.1,
Python 3.13.5. Kompilowano z dwoma równoległymi zadaniami, bez dostępu do CAN,
bez zmian usług, konfiguracji magistrali, zasilania i aktywnego kiosku.

| Kontrola | Wynik |
|---|---|
| 91 importowanych plików vs przypięty commit V2 | PASS: bajty, SHA-256, Git blob i inwentarz |
| Oryginalny skaner granic CORE | PASS |
| Debug, GCC / ARM64 | 23/23 CTest |
| Release, GCC / ARM64 | 23/23 CTest |
| Generic, GCC / ARM64 | 23/23 CTest |
| ASan/UBSan, GCC / ARM64, bez vptr | 23/23 CTest |
| Symbole i inicjalizacja statyczna, Debug / Release / Generic | PASS: po 28 plików obiektowych |
| Samodzielna biblioteka, testy wyłączone | Build PASS, bez wymagania Pythona |
| Nowe testy negatywne narzędzi | 11/11 unittest, w ramach pozycji CTest guard_regression |

23 pozycje CTest to 20 przejętych zestawów CORE i trzy kontrole repozytorium.
Nie są to 23 testy sprzętowe. Generic jest kontrolą konfiguracji grafu budowania,
nie dowodem działania na niezbadanym OS lub MCU.

Pierwsze uruchomienie nowej kontroli binarnej Generic wykazało, że CMake generuje
pliki `.obj`, podczas gdy narzędzie początkowo szukało tylko `.o`. Poprawiono
narzędzie APP, aby sprawdzało oba rozszerzenia; kontrola po poprawce przeszła.
Kod i testy CORE nie zostały zmienione.

## CI

Przy pierwszym zapisie tego dokumentu przygotowano workflow z 9 wariantami:
5 Linux oraz 4 MSVC. Jego uruchomienie wymaga opublikowania pierwszej gałęzi.
Wynik CI zostanie dopisany po rzeczywistym zakończeniu, z identyfikatorem runu
i testowanym commitem. Samo istnienie YAML nie stanowi wyniku PASS.

Workflow pomija zmiany wyłącznie dokumentacyjne. Wyniki opisujące zakończony
run mogą być dopisane bez ponownej kompilacji tych samych źródeł; zawsze podajemy
SHA rzeczywiście przetestowanego kodu.

## Czego nie wykonano i czego nie ma

Brak hosta, loadera pakietów, publicznego SDK, izolowanych workerów, API i WebGUI.
Brak nowych adapterów sprzętowych i modułu SAC. Nie wykonano połączenia z DUT,
kasowania DTC, testu EGR/VGT, sterowania Power Managerem ani wdrożenia na CM5.
Nie ustanowiono produkcyjnego `main` i nie dokonano merge.

Import nie jest ponowną certyfikacją SAE/ISO, bezpieczeństwa funkcjonalnego ani
hard-real-time. Dawne gate'y normatywne zależne od dokumentacji V2 nie zostały
przeniesione i nie otrzymują tutaj statusu PASS. Ich zakres będzie rozpatrywany
przy rzeczywistych zmianach i funkcjach, z osobnymi dowodami.

## Odtwarzanie

Polecenia w [BUILD_AND_TEST.md](BUILD_AND_TEST.md).
Inwentarz i źródło w [PROVENANCE.md](PROVENANCE.md).
Nie opieramy statusu APP na historycznych raportach V1/V2.
