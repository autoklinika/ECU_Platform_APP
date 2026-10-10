# ECU Platform APP

Samodzielna, modułowa platforma laboratoryjna do naprawy i testowania elektroniki
**TRUCK / AGRI / OHV**. DUT oznacza badane urządzenie: ECU, aktuator EGR/VGT,
sensor, bramę lub inny moduł. Produkt nie jest wyłącznie testerem diagnostycznym.

## Stan repozytorium

**Fundament: CORE V2, jego testy oraz nowa dokumentacja architektury.**
Nie ma jeszcze hosta aplikacji, instalatora pakietów, API, WebGUI ani nowego
połączenia z fizycznym DUT. Opis architektury nie jest deklaracją ich wdrożenia.
Aktualne wyniki i ograniczenia: [Weryfikacja](docs/VALIDATION.md).

Przejęto bez zmian 69 plików `src/core_v2/`, 20 zestawów testów C++ oraz 2 pliki
kontroli architektury z `autoklinika/ECU_Platrorm_V2`, commit
`29666b5659d52ad1660bbef8e202dc12507bb221`. Kompilacja i testy nie potrzebują
starego repozytorium ani dostępu do sieci. [Pochodzenie](docs/PROVENANCE.md).

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
Powstaje biblioteka `ECU::core_v2` i programy testowe, nie gotowa aplikacja ECU.

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
