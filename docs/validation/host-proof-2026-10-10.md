# Walidacja kroku 1 — host bez sprzętu

Data: 2026-10-10. Gałąź: `feature/minimal-app-host-20261010`.
Baza: `9915446b65985369b50b00a0a871f5b71ae0d111`.

## Zakres

Nowy host, tymczasowy manifest dowodu, katalog i lifecycle modułów, polityka
operacji, CLI oraz dwa neutralne pakiety. Opis: [HOST_PROOF.md](../HOST_PROOF.md).
CORE, jego 20 zestawów testów i manifest importu są niezmienione.

## Weryfikacja lokalna

Środowisko: CM5, Linux aarch64, GCC 14.2.0, CMake 3.31.6, Ninja 1.12.1,
Python 3.13.5. Wszystkie buildy z maksymalnie dwoma zadaniami równoległymi.

| Weryfikacja | Wynik |
|---|---|
| Debug / GCC / ARM64 | 28/28 CTest PASS |
| Release / GCC / ARM64 | 28/28 CTest PASS |
| Generic / GCC / ARM64 | 28/28 CTest PASS |
| ASan/UBSan / GCC / ARM64, bez vptr | 28/28 CTest PASS |
| Integralność 91 importowanych plików | PASS |
| Symbole CORE i inicjalizacja statyczna, Debug/Release/Generic | PASS |
| Build samego CORE, host i testy wyłączone | PASS |

28 pozycji CTest = 20 oryginalnych zestawów CORE, 3 wcześniejsze kontrole repo
oraz 5 hosta: manifest, lifecycle/polityka, SDK standalone, CLI i granice.
CLI obejmuje 12 testów unittest; testy granic obejmują 4 testy unittest.
To liczby zestawów/testów programowych, nie testów urządzenia.

Zrealizowany scenariusz w jednym procesie: rejestracja echo i failure,
aktywacja obu, kontrolowany błąd failure, poprawne echo i niezależne migawki.
Nowy dowolny identyfikator pakietu oraz nowa nazwa operacji są tworzone przez
test na dysku i uruchamiane na wcześniej skompilowanym hoście.

## CI

Wynik pełnej macierzy Linux/Windows zostanie zapisany po jej zakończeniu ze
wskazaniem faktycznie testowanego commita. Lokalne PASS nie oznacza Windows PASS.

## Granice wyniku

Wyłącznie testy bez sprzętu. Nie otwierano CAN, RS485, zasilania, kiosku ani
usług. Brak połączenia hosta z runtime CORE, brak trwałego/podpisanego instalatora,
IPC/sandboxa dodatków, API HTTP, WebGUI, sesji DUT oraz operacji na prawdziwym ECU.
Testowane kontrolowane błędy/wyjątki nie dowodzą izolacji crasha natywnego ani
odporności na zawieszenie hosta. Generic oznacza graf kompilacji, nie fizyczny
port na inny OS. Linux/Windows CI jest oddzielne od testu urządzenia.
