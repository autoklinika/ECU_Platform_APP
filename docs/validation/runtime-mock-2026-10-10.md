# Walidacja runtime na atrapie CAN

Data: 2026-10-10. Gałąź: `feature/core-runtime-mock-20261010`.
Baza: `e79ec641028a41dd1ef6822e72fa7dd0fc7c5329`.
Zakres: [RUNTIME_MOCK.md](../RUNTIME_MOCK.md).

## Testy

37 pozycji CTest: 20 oryginalnych zestawów CORE, 3 kontroli repozytorium,
5 hosta oraz 9 runtime/atrapy. Ostatnia pozycja zawiera trzy testy realnego CLI.

Scenariusze: trwała sesja i powtórne odczyty, konflikt zasobów, aliasy kanału,
stare generacje i sesje, cancel przed i po TX, zatrzymanie przy pending,
timeout przed pierwszym krokiem i po TX, spóźnione odpowiedzi, limit RX,
would_block TX, RX overflow, bus-off, błąd open i receive, awaria domeny czasu,
nieudany cleanup ze starym lease, jawne recovery, destrukcja oraz integracja hosta.
Dodatkowo: awaria odczytu zegara bezpośrednio przed TX blokuje nadanie; znacznik
RX z przyszłości nie jest zatwierdzany jako aktualna obserwacja.

Katalog i SDK nadal nie znają klas CORE; źródłowa kontrola granic jest dodatkowym
sprawdzeniem, nie dowodem izolacji pamięci lub sandboxa.

## Weryfikacja lokalna

CM5, Linux ARM64, GCC 14.2.0, CMake 3.31.6, Ninja 1.12.1, Python 3.13.5.
Maksymalnie dwa równoległe zadania kompilacji.

| Wariant | Wynik |
|---|---|
| Debug | 37/37 CTest PASS |
| Release | 37/37 CTest PASS |
| Generic (graf kompilacji, nie inny fizyczny OS) | 37/37 CTest PASS |
| ASan/UBSan, bez vptr | 37/37 CTest PASS |
| 91 plików importu CORE | Identyczne |
| Kontrola niezależności binarnej CORE | PASS |
| Host bez testów i bez wsparcia atrap | Build PASS |

## CI

Pełna macierz Linux/Windows zostanie sprawdzona dla publikowanego commita.
Nie jest jeszcze oznaczona jako PASS w tej rewizji dokumentu.
Nie uznajemy wyniku Linux za wynik Windows.

## Korekta wykryta przez testy

Pierwsza atrapa deklarowała zerowy limit niepewności znacznika RX. Kontrakt
ICanDriver wymaga dodatniego limitu, więc CORE prawidłowo odrzucił otwarcie.
Poprawiono deklarację atrapy i dodano jej sprawdzenie; CORE nie był zmieniany.

## Granice

Wszystkie testy bez fizycznego CAN. Czas jest wirtualny, nie zegarem OS.
Nie mierzono rzeczywistego jitteru ani czasów reakcji urządzenia. Nie testowano
SIGKILL, awarii OS, izolacji procesów dodatków ani fizycznego safe-stop.
Żadne polecenie tego zadania nie zmienia usług, kiosku, magistrali ani zasilania.
