# Pochodzenie i kontrola importu CORE V2

## Źródło

- Repozytorium: `autoklinika/ECU_Platrorm_V2` (pisownia źródłowa).
- Ref przy imporcie: produkcyjny `main`.
- Commit: `29666b5659d52ad1660bbef8e202dc12507bb221`.
- Git tree `src/core_v2`: `71b04ab0da332b8466b814bac99c5685e5a91901`.
- Data importu: 2026-10-10.

[Źródłowy CORE](https://github.com/autoklinika/ECU_Platrorm_V2/tree/29666b5659d52ad1660bbef8e202dc12507bb221/src/core_v2)
pozostaje niezmieniony. Import nie zapisuje do starego repo ani nie zmienia
aktywnych worktree i usług.

## Dokładny zakres

| Zakres | Plików | Zasada |
|---|---:|---|
| `src/core_v2/` | 69 | Identyczne bajty i ścieżki, włącznie z lokalnym CMake |
| `tests/core_v2_*_tests.cpp` | 20 | Całe źródła oryginalnych zestawów testów |
| `scripts/check_core_v2_architecture.py` i `.sh` | 2 | Identyczne kontrole granic źródłowych |
| Łącznie w manifeście importu | 91 | Dla każdego pliku rozmiar, SHA-256 i Git blob SHA-1 |

Manifest maszynowy: [`provenance/core_v2_import.json`](../provenance/core_v2_import.json).
Nowe główne CMake, CMake testów, CI, testy narzędzi, dokumentacja i reguły projektu
należą do APP. Nie są przejętym kodem CORE.

## Co jest potrzebne do działania kopii

CORE ma własne nagłówki i źródła C++17. Oryginalne testy korzystają z tych nagłówków
oraz biblioteki standardowej; nie potrzebują starego Bench Runtime ani Linuxa.
Dodano samodzielny build i kontrolę kompletności listy plików.

Nie skopiowano: `src/core` (legacy), Bench, DUT Profile, aplikacji SAC, API,
WebGUI, adapterów Linux, agentów, skryptów instalacyjnych, starych buildów i
prywatnych logów. Nie ma submodułu ani pobierania kodu podczas konfiguracji CMake.

Stare dokumenty zamrożenia, macierze normatywne i raporty PASS nie są dokumentacją
nowego projektu. Zachowujemy do nich odniesienia, ale nie dziedziczymy statusu
całej aplikacji. Stary szeroki skrypt walidacyjny ma zależności od struktury V2;
nie został przeniesiony. Nowe repo uruchamia wszystkie 20 zestawów CORE oraz
niezależne kontrole importu i architektury. Dawne gate'y normatywne nie są tu
odtworzone ani przedstawiane jako zaliczone.

## Weryfikacja

```sh
python3 tools/verify_core_import.py
python3 scripts/check_core_v2_architecture.py src/core_v2
```

Kontrola importu porównuje bajty, rozmiary, SHA-256, Git blob SHA-1 i zamknięty
inwentarz. Odrzuca brakujące i dodatkowe pliki, duplikaty oraz ucieczkę ścieżki.
Manifest jest dowodem wskazanego pochodzenia, nie podpisem kryptograficznym ani
systemem zaufania do zewnętrznych pakietów. Zmiana pliku i manifestu razem wymaga
przeglądu; sam skrypt nie zastępuje kontroli zmian w Git.

Przyszła rewizja CORE wymaga [procedury rozwoju](CORE_EVOLUTION.md), zachowania
historii importu i jawnej listy lokalnych zmian. Nigdy nie maskujemy modyfikacji
przez ciche przeliczenie sum kontrolnych.

## Referencje do linii V1/V2

- [Kontrakty i ograniczenia CORE V2](https://github.com/autoklinika/ECU_Platrorm_V2/blob/29666b5659d52ad1660bbef8e202dc12507bb221/docs/CORE_V2_ENGINEERING_FREEZE_2026-10-07.md)
- [V1.8 VirtualCockpit](https://github.com/autoklinika/ecu_platform/blob/99a66fc82d39acf10acb456adac3983e02039113/src/virtual_cockpit/VirtualCockpit.cpp)
- [Incydent EGR: logika testu nadal w GUI](https://github.com/autoklinika/ecu_platform/blob/785784965f2f6de9e8bf32c952b51fd2084ddd2a/docs/reports/EGR_AUTOTEST_GUI_THREAD_REGRESSION_INCIDENT_PL.md)

Referencje dokumentują pochodzenie decyzji. Nie są zależnościami wykonawczymi.
