# Budowanie i testy fundamentu

## Wymagania

C++17, CMake >= 3.25. Przy włączonych testach wymagany Python >= 3.9.
Ninja jest opcjonalnym generatorem. Nie potrzeba Qt, Node, Chromium, CAN,
uprawnień root ani innych repozytoriów. Budowanie po sklonowaniu jest offline;
workflow CI pobiera wybrane narzędzia standardowymi akcjami GitHub.

CORE zachowuje oryginalne opcje ostrzeżeń, brak RTTI i wyjątki wyłączone dla
GCC/Clang. Reszta aplikacji nie jest jeszcze zbudowana. Zasady ograniczeń
ścieżki krytycznej nie oznaczają zakazu kontenerów STL we wszystkich przyszłych
narzędziach, GUI, raporcie albo kodzie testowym.

## Linux / CM5

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 2
ctest --test-dir build/debug --output-on-failure --no-tests=error

cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure --no-tests=error
```

Usuń `-G Ninja`, jeśli używasz domyślnego generatora. Nie mieszaj generatorów
w tym samym katalogu build. Liczbę zadań dobierz do dostępnej pamięci.

## Windows / MSVC

Uruchom w środowisku z Visual Studio 2022 (C++ build tools), CMake i Pythonem:

```powershell
cmake -S . -B build/msvc-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/msvc-x64 --config Debug --parallel 2
ctest --test-dir build/msvc-x64 -C Debug --output-on-failure --no-tests=error
cmake --build build/msvc-x64 --config Release --parallel 2
ctest --test-dir build/msvc-x64 -C Release --output-on-failure --no-tests=error
```

Dla Win32 użyj odrębnego katalogu i `-A Win32`. Nie zmienia to kodu CORE.
Jeżeli środowisko Pythona używa lokalnego kodowania zamiast UTF-8, ustaw
`$env:PYTHONUTF8 = "1"` przed uruchomieniem kontroli źródeł. CI ustawia to jawnie.

## Same źródła biblioteki

```sh
cmake -S . -B build/library -DECU_APP_BUILD_TESTS=OFF
cmake --build build/library --parallel 2
```

Powstaje statyczna biblioteka `ECU::core_v2`. Wyłączenie testów usuwa wymaganie
Pythona z konfiguracji. Nie istnieje jeszcze komenda startu hosta ani instalator.

## Co uruchamia CTest

20 oryginalnych zestawów testów CORE oraz trzy kontrole repozytorium: integralność
importu, oryginalny skaner architektury i nowy zestaw negatywnych testów narzędzi.
Łącznie 23 pozycje CTest; liczba ta nie jest liczbą wszystkich asercji ani
scenariuszy wewnątrz testów. Zestaw `guard_regression` zawiera 11 testów unittest
oraz podprzypadki zabronionych konstrukcji.

```sh
ctest --test-dir build/debug -L core --output-on-failure --no-tests=error
ctest --test-dir build/debug -L repository --output-on-failure --no-tests=error
python3 tools/verify_core_import.py
python3 scripts/check_core_v2_architecture.py src/core_v2
python3 tests/repo/test_guards.py
```

Skaner jest konserwatywną analizą źródeł, nie dowodem pełnej zgodności języka,
standardu protokołu ani bezpieczeństwa. Kopia pozostaje byte-exact, a jego
ograniczenia nie są ukrywane pod nową nazwą.

## Sanitizery

```sh
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DECU_APP_SANITIZERS=ON
cmake --build build/sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize --output-on-failure --no-tests=error
```

Opcja działa z GCC/Clang. Wyłączony jest wyłącznie sanitizer `vptr`, ponieważ
CORE używa `-fno-rtti`; pozostałe ASan/UBSan pozostają aktywne. Artefakty
instrumentowane mają własne zależności runtime, więc nie stosujemy do nich
tego samego audytu symboli co do biblioteki bez instrumentacji.

## Konfiguracja bez docelowego Linuxa

```sh
cmake -S . -B build/generic -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Generic
cmake --build build/generic --parallel 2
ctest --test-dir build/generic --output-on-failure --no-tests=error
```

To kontrola niezależności grafu budowania od CMAKE_SYSTEM_NAME=Linux.
Testy nadal uruchamiają się na hoście; wynik nie dowodzi pracy na MCU ani
nie zastępuje prawdziwego builda Windows.

## CI

Jeden workflow `core-foundation.yml` obejmuje GCC Debug/Release, Clang Debug,
Clang ASan/UBSan, Generic oraz MSVC Win32/x64 Debug/Release. Akcje są przypięte
do SHA. Workflow nie wdraża, nie wydaje poleceń sprzętowych i ma tylko odczyt
repozytorium. Każdy wariant uruchamia te same 23 pozycje CTest.

Dodatkowo Linux bez sanitizerów kontroluje symbole biblioteki i brak dynamicznej
inicjalizacji statycznej za pomocą `tools/check_core_binary.py`. Kontrola wymaga
`nm` i `readelf`; brak narzędzia jest błędem, nie cichym PASS.

Faktycznie wykonane warianty i otwarte obszary są opisane w
[VALIDATION.md](VALIDATION.md), a źródło kodu w [PROVENANCE.md](PROVENANCE.md).
