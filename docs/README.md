# Dokumentacja ECU Platform APP

Dokumenty opracowano od nowa na podstawie ustaleń właściciela i analizy V1/V2
w rozmowie z 10 października 2026 r. Nie są kopią dawnych etapów projektu.

| Dokument | Zawartość |
|---|---|
| [Architektura](ARCHITECTURE.md) | Warstwy, przepływy, sesje, czas i granice awarii |
| [Kontrakt modułów](MODULE_CONTRACT.md) | Model instalowalnych aplikacji, zależności, zasoby i SDK |
| [Rozwój CORE](CORE_EVOLUTION.md) | Postępowanie, gdy nowa aplikacja wymaga brakujących możliwości |
| [Decyzje](DECISIONS.md) | Zaakceptowany kierunek, uzasadnienia i wycofane propozycje |
| [Zasady projektu](GOVERNANCE.md) | Izolacja, main, testy sprzętowe i sposób prowadzenia prac |
| [Plan](ROADMAP.md) | Małe kroki z działającym wynikiem i kryteriami akceptacji |
| [Pochodzenie](PROVENANCE.md) | Dokładny import CORE, wyłączenia i integralność |
| [Budowanie i testy](BUILD_AND_TEST.md) | Odtwarzalne polecenia Linux/Windows oraz CI |
| [Weryfikacja](VALIDATION.md) | Faktycznie wykonane testy oraz nadal otwarte obszary |

## Jak odczytywać status

**Ustalony kierunek** to decyzja architektoniczna z rozmowy, a nie istniejący kod.
**Projekt kontraktu** to propozycja szczegółowego API, którą sprawdzimy w działającym
hoście. **Zaimplementowane** wymaga plików źródłowych. **Zweryfikowane** wymaga
podanego środowiska i wyniku. Test symulowany nigdy nie oznacza fizycznego PASS.

Przy tej rewizji istnieją CORE, jego testy i narzędzia repozytorium. Pozostałe
komponenty opisane w dokumentacji są kierunkiem dalszej implementacji.

## Bieżąca implementacja

- [Działający host i pakiety bez sprzętu](HOST_PROOF.md).
- [Walidacja kroku 1](validation/host-proof-2026-10-10.md).
