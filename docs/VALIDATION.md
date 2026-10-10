# Weryfikacja fundamentu

Data: 2026-10-10. Historyczny wynik kroku 0 w APP, nie dawnego wydania V2.
Aktualny host opisuje [walidacja kroku 1](validation/host-proof-2026-10-10.md).

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

**CI zakończone: 9/9 wariantów PASS.**

- Testowany commit APP: `5b8f4e77c640739dba96964729d3e4e04e635810`.
- Run: [38037464682](https://github.com/autoklinika/ECU_Platform_APP/actions/runs/38037464682).
- Linux: GCC Debug, GCC Release, Clang Debug, Clang ASan/UBSan i GCC Generic.
- Windows: MSVC Win32 oraz x64, każdy w Debug i Release.
- GitHub Actions potwierdza sukces wszystkich kroków `Build` i `Verify offline`.
  Każdy wariant ma skonfigurowany ten sam zestaw 23 pozycji CTest z `--no-tests=error`.
- Maszynowy zapis metadanych runu i kroków: [bootstrap-2026-10-10.json](validation/bootstrap-2026-10-10.json).

Po tym runie dopisano wyłącznie dokumentację i zapis wyników. Źródła CORE,
testy, CMake, narzędzia i workflow pozostają takie jak w testowanym commicie.

Workflow pomija zmiany wyłącznie dokumentacyjne. Wyniki opisujące zakończony
run mogą być dopisane bez ponownej kompilacji tych samych źródeł; zawsze podajemy
SHA rzeczywiście przetestowanego kodu.

## Czego nie wykonano i czego nie było w kroku 0

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
