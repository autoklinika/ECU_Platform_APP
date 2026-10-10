# Krok 1 — działający host i pakiety bez sprzętu

Status: **zaimplementowany dowód kontraktu**. Nie jest to gotowy instalator
produkcyjny, runtime stanowiska ani stabilne ABI dodatków. Wyniki:
[walidacja hosta](validation/host-proof-2026-10-10.md).

## Co rzeczywiście działa

`ecu_app_host` to samodzielny program C++17 uruchamiany bez GUI. Jeden proces
obsługuje kolejne komendy ze standardowego wejścia i utrzymuje katalog w pamięci.
Można zarejestrować pakiet z dysku, sprawdzić katalog, nadać uprawnienie operacji,
aktywować moduł, wykonać operację, odebrać wynik, wyłączyć i usunąć moduł.
Wyłączenie jednego modułu nie wyłącza pozostałych.

Host nie zna ID przykładów ani nazw operacji. Punkt złożenia programu dostarcza
mu dwie zaufane usługi `demo.echo` i `demo.fail`. Pakiet sam określa swoje ID i
nazwy operacji mapowane na te istniejące możliwości. Dodanie nowego takiego
pakietu nie wymaga ponownej kompilacji ani edycji hosta lub innych pakietów.
Dodanie nowej klasy usługi wymaga implementacji platformy — to celowa granica.

Nie ma arbitralnego wykonywania kodu z pakietu, ładowania `.so`/`.dll`, hooków,
sieci, GUI, dostępu do CAN/RS485, przekaźników ani usług systemowych.
Pliki inne niż manifest są ignorowane, nie wykonywane. Operacja `demo.fail`
zwraca kontrolowany błąd; nie jest symulacją uszkodzenia fizycznego DUT.

## Granice kodu i budowania

| Target | Odpowiedzialność | Zależności |
|---|---|---|
| `ECU::app_sdk` | struktury manifestu, wyniku i migawki | C++17, bez hosta i CORE |
| `ECU::app_host` | walidacja, katalog, lifecycle, polityka i wywołania | wyłącznie SDK |
| `ecu_app_host` | CLI, odczyt pliku, złożenie usług demonstracyjnych | host |
| `ECU::core_v2` | niezmieniony import rdzenia | brak zależności od APP |

Host **nie jest jeszcze połączony z CORE**. To krok 2; nie dodano drugiego
CanBusRuntime, schedulera, EventBus ani ResourceManagera. Katalog pakietów to
nie aktywny rejestr obiektów CORE. Test kompilacji SDK linkuje tylko SDK,
a testy hosta wyłącznie host — nie wykorzystują Qt, CORE ani adaptera Linux.

## Tymczasowy manifest dowodu

Na potrzeby kroku 1 używamy pliku `manifest.ecu`: płaskie rekordy `key=value`,
bez skryptów, zagnieżdżeń, odwołań do plików i escape sequences. To świadomie
mały format do weryfikacji kontraktu, **nie docelowy podpisany `.ecupkg` ani
zamiennik JSON jako uniwersalnego języka**. Nie wprowadzamy zależności JSON
ani własnego ogólnego parsera JSON wyłącznie dla tego testu.

Przykład:

```ini
schema=1
id=org.ecu.example.echo
version=1.0.0
api=1.0
requires=demo.echo
operation.ping=demo.echo
```

`schema`, `id`, `version`, `api`, `requires` są obowiązkowe. Każda operacja ma
nazwę i usługę; wszystkie użyte usługi muszą być wymienione w `requires` i
istnieć w hoście. `api=1.0` oznacza wymagany major 1 i minimalny minor 0.
Obsługiwane API tego eksperymentalnego kontraktu to 1.0; wymagana wyższa wersja
jest odrzucana, nie interpretowana jako zgodna na podstawie podobnych nazw.
Wersja pakietu ma trzy składniki liczbowe 0..65535, bez prerelease i metadanych.

Nazwy: 1..64 znaki, małe litery ASCII, cyfry, `.`, `_`, `-`; pierwszy i ostatni
znak alfanumeryczny, bez `..`. Brakujące/nieznane pola, duplikaty i niezgodny
format są odrzucane. Manifest nie może zawierać pola przyznającego uprawnienia.
Komentarz zaczyna się od `#` na początku rekordu; zakończenia LF/CRLF są obsługiwane.

Limity: manifest 8192 bajty i 64 linie; linia 512 bajtów; do 16 wymaganych usług
oraz 16 operacji; katalog do 32 modułów. Tekst wejścia i wyniku operacji to do
512 drukowalnych znaków ASCII. Komenda CLI do 2048 bajtów. Wielojęzyczne dane
DUT/UI, podpisy, zależności pakietów, opcjonalne capability, migracje i wire API
pozostają do dalszego projektu. Ten format nie jest obietnicą dożywotniej zgodności.

## Uprawnienia, stan i generacje

Zarejestrowany moduł zaczyna jako `disabled` i bez uprawnień. `activate` nie
uruchamia operacji. `allow` jest decyzją zaufanego operatora hosta; pakiet nie
może sam go wywołać ani dostać uprawnień przez manifest. Zgoda jest przypisana
do konkretnego modułu i operacji, nie globalnie do wszystkich modułów.

Każda aktywacja dostaje nową rosnącą generację w obrębie procesu. `run` i
`deactivate` wymagają zgodnej generacji; stare żądanie nie oddziałuje na nową
aktywację, także po usunięciu i ponownym zarejestrowaniu tego samego ID.
Generacja nie jest sekretem ani mechanizmem uwierzytelniania. Nie jest jeszcze
identyfikatorem sesji DUT odpornym na restart procesu; ten kontrakt powstaje
przy sesjach i zewnętrznym API.

Przyjęta operacja otrzymuje rosnące `operation_id`, generację i wynik. Odrzucona
operacja ma ID 0 i nie wykonuje usługi. Host trzyma ostatni wynik oraz licznik
wykonanych operacji danego modułu. Migawki są kopiami, nie dostępem do jego
pamięci. Wynik błędu usługi nie niszczy innych modułów ani nie zamienia się w
sukces. Wyjątek C++ z zaufanej usługi ma kontrolowany wynik, bez publikowania
szczegółów wyjątku.

To wykonanie krótkich, synchronicznych operacji bez sprzętu. Nie ma jeszcze
kolejki asynchronicznej, anulowania trwających operacji, watchdogów, deadline'ów,
trwałego dziennika, idempotencji retries ani IPC. Zawieszający się lub uszkadzający
pamięć kod natywny w tym samym procesie **nie jest odizolowany**. Nie wolno
przedstawiać tego testu jako sandboxa lub dowodu terminowości.

Host jest single-caller. Ponowne wejście ze zaufanego callbacku w mutujące metody
jest odrzucane jako `busy`; nie ma obsługi równoległego dostępu z wielu wątków.

## Rejestracja i aktualizacja

`install` czyta wyłącznie `manifest.ecu` we wskazanym katalogu, sprawdza go i
kopiuje opis do pamięci. Dalsza zmiana pliku nie zmienia aktywnego opisu.
Nie rozpakowuje archiwum, niczego nie zapisuje na dysku i nie uruchamia modułu.
Katalog znika po zamknięciu hosta; nie jest to jeszcze trwała instalacja.

Czytnik odrzuca symlink katalogu pakietu i symlink samego manifestu oraz pliki
inne niż zwykłe. Odczyt jest ograniczony rozmiarem. Nie deklarujemy odporności
na wrogą równoległą podmianę ścieżek ani pełnej izolacji systemu plików.

Duplikat ID przy `install` daje `duplicate_id`, a nie ciche nadpisanie.
`replace` wymaga spodziewanej wersji poprzedniej i wyłączonego modułu. Cały nowy
opis przechodzi weryfikację przed podmianą w katalogu. Błąd nie zmienia starego
modułu. Udana podmiana kasuje stare uprawnienia i wyniki; ponowna zgoda jest jawna.
Można przygotować inny, wyłączony moduł, gdy trwa praca niezależnego przykładu.
Aktualizacja i usunięcie aktywnego modułu są zawsze blokowane.

## Uruchomienie

Po zwykłym buildzie opisanym w [BUILD_AND_TEST.md](BUILD_AND_TEST.md):

```sh
./build/debug/apps/host/ecu_app_host --help
./build/debug/apps/host/ecu_app_host
```

Na Windows z generatorem Visual Studio: `build/debug/apps/host/Debug/ecu_app_host.exe`.
Wpisz kolejne komendy ze wskazaniem generacji otrzymanej z `activate`:

```text
install "packages/examples/echo"
activate org.ecu.example.echo
run org.ecu.example.echo 1 ping "without permission"
allow org.ecu.example.echo ping
run org.ecu.example.echo 1 ping "hello"
list
deactivate org.ecu.example.echo 1
remove org.ecu.example.echo
quit
```

Pierwszy `run` zostanie odrzucony (`permission_denied`), drugi zwróci `hello`.
Generacja 1 dotyczy pierwszej aktywacji w świeżym procesie, nie stałego ID.
`list` zwraca operacje, uprawnienia, stan i ostatni wynik każdego modułu.

Odpowiedzi to osobne obiekty JSON. Komendy są tekstowe, **nie JSON**; cudzysłowy
w komendach obsługują escaped quote i backslash. CLI jest lokalnym interfejsem
administracyjnym dla testu, nie uwierzytelnionym API dla przeglądarki. Błędy
operacji są w polu `status`, nie w kodzie zakończenia procesu interaktywnego.
EOF/`quit` normalnie kończy proces z 0; błędne argumenty uruchomienia lub awaria
wejścia/zasobów dają 2.

## Co sprawdzają testy

- Manifest: kompletność, format, limity, duplikaty, obce pola, niezadeklarowana usługa.
- Host: zgodność API, brak capability, default-deny, grant/revoke, nowa generacja,
  wyniki, kopie snapshotów, limity katalogu, update/remove aktywnego, CAS wersji,
  wyzerowanie grants po update, kontrolowana porażka i re-entry callbacku.
- Realny program: pełen lifecycle w jednym procesie, nowy dowolny pakiet/nazwa
  operacji bez rebuild, dwa moduły i błąd jednego, odrzucenie hooka w manifeście,
  brak wykonania plików dodatku, ścieżki ze spacjami, JSON quoting i limity wejścia.
- Granice: osobna kompilacja SDK; zakaz zależności SDK od hosta/CORE;
  brak bezpośredniego sprzętu, GUI, subprocessów i nazw OEM w hoście.
- Cały niezmieniony CORE i wcześniejsze kontrole repozytorium.

Testy statyczne są kontrolami źródeł, nie formalnym dowodem bezpieczeństwa.
Test symlinka wykonuje się tam, gdzie system pozwala go utworzyć; bez tego
uprawnienia na Windows sprawdzenie symlinka nie stanowi dowodu jego odrzucenia.
