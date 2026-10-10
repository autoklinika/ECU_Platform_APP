# ECU Platform APP

Samodzielna, modułowa platforma laboratoryjna do naprawy i testowania elektroniki
**TRUCK / AGRI / OHV**. DUT oznacza badane urządzenie: ECU, aktuator EGR/VGT,
sensor, bramę lub inny moduł. Produkt nie jest wyłącznie testerem diagnostycznym.

## Stan repozytorium

**Host aplikacji oraz integracja z runtime CORE na atrapie CAN.**
Katalog, kontrola uprawnień i lifecycle pakietów działają bez GUI. Nowy dowód
utrzymuje sesję, wykonuje nieblokujące operacje z generacją/ID, używa zasobów,
anulowania i watchdogów CORE oraz kończy transport po awarii.
[Zakres i uruchomienie](docs/RUNTIME_MOCK.md),
[wyniki](docs/validation/runtime-mock-2026-10-10.md).

CORE V2, jego 20 zestawów testów i 2 kontrole granic — 91 plików — są nadal
identyczne ze wskazanym importem. [Pochodzenie](docs/PROVENANCE.md).
Nie ma jeszcze rzeczywistego adaptera, executora OS, API HTTP, WebGUI ani
połączenia z fizycznym DUT. To dowód integracji programowej, nie wydanie stanowiska.

## Kierunek zaakceptowany przez właściciela

Mały host C++ i wspólny zaufany runtime, instalowalne pakiety aplikacji oraz
niezależne WebGUI. Oddzielny proces tylko tam, gdzie potrzebna jest izolacja.
Moduły znają publiczne kontrakty, nie implementacje innych modułów. Jeden
właściciel fizycznego kanału CAN. Sterowanie, sekwencje, pomiary czasu i nadzór
nie zależą od renderowania, strony GUI ani żądania HTTP.

CORE pozostaje stabilnym fundamentem, ale nie jest zamrożony na zawsze.
Brakujące możliwości dodajemy na właściwej warstwie; zmiana CORE wymaga
uzasadnienia, kontroli zgodności i ponownej pełnej weryfikacji.

## Budowanie bez GUI i bez sprzętu

Wymagania: kompilator C++17, CMake >= 3.25; do testów Python >= 3.9.
Przykład z generatorem domyślnym:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --config Debug --parallel 2
ctest --test-dir build/debug -C Debug --output-on-failure --no-tests=error
```

Na Windows można użyć generatora Visual Studio; `--config` / `-C` wybiera
konfigurację. Szczegóły: [Budowanie i testy](docs/BUILD_AND_TEST.md).
Powstają biblioteka `ECU::core_v2`, host `ecu_app_host` i programy testowe.
Host nie wykonuje operacji sprzętowych. Sam CORE bez hosta i testów można
zbudować z `-DECU_APP_BUILD_HOST=OFF -DECU_APP_BUILD_TESTS=OFF`.
Przykładowe pakiety: `packages/examples/echo` i `packages/examples/failure`.

## Dokumentacja

Zacznij od [indeksu](docs/README.md), następnie
[architektury](docs/ARCHITECTURE.md),
[kontraktu modułów](docs/MODULE_CONTRACT.md) i
[zasad rozwoju CORE](docs/CORE_EVOLUTION.md).
[Decyzje i odrzucone warianty](docs/DECISIONS.md) zachowują tok ustaleń z rozmowy.
[Plan](docs/ROADMAP.md) prowadzi od sprawdzalnego hosta do SAC i kolejnych modułów.

## Reguły pracy

Gałęzie robocze; zmiany `main` tylko za jawną zgodą właściciela na konkretny
zestaw zmian. Testy domyślnie bez sprzętu. Żadnego automatycznego wdrożenia na
CM5, nadawania CAN, kasowania DTC ani uruchamiania aktuatora. Zobacz
[AGENTS.md](AGENTS.md) i [zasady projektu](docs/GOVERNANCE.md).
