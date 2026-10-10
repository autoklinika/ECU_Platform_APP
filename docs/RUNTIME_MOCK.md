# Host + CORE: runtime na atrapie CAN

Data: 2026-10-10. Baza: `e79ec641028a41dd1ef6822e72fa7dd0fc7c5329`.
Zakres: działająca integracja programowa bez fizycznego urządzenia.
Nie jest to wdrożony serwis stanowiska, moduł SAC ani test sprzętowy.

## 1. Granice kodu

- `src/app_sdk/runtime.hpp` (w drzewie include): eksperymentalny kontrakt
  źródłowy API 1.1, bez nagłówków CORE, hosta, systemu i sprzętu.
- `src/app_host`: katalog, autoryzacja operacji, generacja aktywacji i wyniki.
  Zna tylko `IRuntimePort`; nadal nie linkuje CORE ani sterownika.
- `src/app_runtime`: zaufana kompozycja `CanBusRuntime`, `ResourceManager`,
  `CancellationSource` i dwóch `DeadlineWatchdog` z istniejącego CORE.
- `support/testing`: jawna atrapa kanału, sterownika i zegara.
- `apps/runtime_mock`: uruchamialny dowód integracji, wczytujący pakiet z dysku.

Nie dodano drugiego ogólnego arbitra, schedulera, ResourceManagera ani stosu CAN.
CORE i wszystkie 91 plików importu pozostają identyczne z bazą.
Katalog instalacji jest odrębny od konfiguracji aktywnej sesji.

API 1.0 dla dotychczasowych krótkich usług jest zachowane. Nowy pakiet wymaga
API 1.1. Przy deklaracji przyszłego API 1.2 host odmawia rejestracji. To nadal
kontrakt C++ kompilowany razem z hostem, nie stabilne ABI DLL ani format IPC.

## 2. Wykonanie i operacje

Wszystkie wywołania hosta i runtime należą do jednego, zewnętrznie serializowanego
kontekstu wykonania. Nie ma tutaj nowych wątków, mutexów, pętli HTTP ani timerów
GUI. `Host::service()` ma być wywoływane przez zaufany executor, nie ekran.
Dowód używa zegara wirtualnego i sam wywołuje kolejne kroki executora.
Implementacja executora OS i jego pomiary pozostają osobną pracą.

`Host::invoke()` wykonuje dotychczasowe kontrole modułu, generacji, uprawnień
oraz operacji. Dla usługi runtime przekazuje nazwane działanie przez port SDK.
Zwrócone `accepted` oznacza wyłącznie przyjęcie: `Result::ok()` nie zwraca wtedy
prawdy. Samo przyjęcie connect nie otwiera nawet atrapy CAN.

Host utrzymuje pending i osobno ostatni wynik zakończony. Dopiero executor
publikuje zakończenie z oryginalnym ID operacji, generacją aktywacji i sesją.
Odczyt pomyślny zawiera czas obserwacji w domenie runtime. Nie jest to fizyczny
pomiar ciśnienia/napięcia; payload mówi jawnie o odpowiedzi atrapy.

Jest jeden slot oczekującej operacji i jeden slot wyniku, nie nieograniczona
kolejka. Nieodebrany wynik blokuje nowe przyjęcie. Jeden moduł nie może mieć
kilku operacji naraz. Niezależna krótka usługa innego modułu nadal działa.
Nie jest to gwarancja odporności na dowolny blokujący kod zaufanej usługi.

## 3. Sesja i zasoby

Przy connect runtime pozyskuje lease `can_channel` z przekazanego wspólnego
ResourceManagera. Następnie buduje CanBusRuntime, rejestruje odbiorcę, zamraża
konfigurację i uruchamia transport. Kolejne odczyty zachowują ten sam stos,
sesję i otwarty sterownik. Nie ma procesu ani DOWN/UP dla każdego odczytu.

Drugi właściciel tego zasobu dostaje `resource_busy`. Dodatkowy test używa
osobnej mapy zasobów i drugiego wrappera tego samego fizycznego kanału: wspólny
arbiter sterownika nadal odrzuca drugie otwarcie. To nie zastępuje przyszłych
uprawnień systemowych dla prawdziwego SocketCAN.

`CanBusRuntime::stop()` zachowuje fizyczny lease według kontraktu CORE.
Dlatego przy końcu całej sesji następuje kontrolowane zniszczenie obiektu bus,
a dopiero potem zwolnienie lease logicznego. Sink żyje dłużej niż bus; sterownik,
zegary i ResourceManager muszą żyć dłużej niż runtime. Runtime żyje dłużej niż Host.

Dezaktywacja aplikacji jest odrzucana, gdy ma pending albo nadal posiada sesję.
Najpierw trzeba zakończyć jej operację i wykonać disconnect/recover. Sama nawigacja
nie jest dezaktywacją i nie istnieje tu zależność od strony GUI.

## 4. Anulowanie, awaria i zakończenie

Anulowanie ma osobny token CORE. Stary ID nie anuluje kolejnego żądania.
Unieważnienie uprawnienia do oczekującej operacji żąda anulowania tej operacji.
Cancel jest obsługiwany przed TX/RX, a shutdown poza zwykłym slotem poleceń.
Żądanie anulowane przed pierwszym krokiem nie nadaje żadnej ramki.

Zwykły timeout odczytu kończy ten odczyt, bez ponownego otwierania zdrowej sesji.
Deadline obejmuje także oczekiwanie przed pierwszym krokiem. Po przekroczeniu
terminu późna odpowiedź nie zmienia timeoutu w sukces.

Bus-off, błąd transportu, utrata RX lub awaria czasu zamykają transport i oznaczają
sesję jako faulted. Lease logiczny pozostaje w kwarantannie aż do jawnego recovery
właściciela. Recovery zwalnia zasób; nie uruchamia samoczynnie ruchu ani połączenia.
Nowe connect tworzy nową sesję. Polecenie ze starej sesji jest odrzucane.

Niepowodzenie zwolnienia lease jest widoczne jako `cleanup_failed`. Runtime nie
zwalnia lease innego właściciela i nie zgłasza udanego disconnect. Test celowo
narusza zewnętrznie własność logiczną, aby sprawdzić ten przypadek.

Shutdown jest terminalny i idempotentny; kończy pending, zamyka bus i zwalnia
własne zasoby. Destruktory są ostatnią linią cleanupu, nie dowodem bezpieczeństwa
po SIGKILL/zawieszeniu OS. Fizycznego safe-stop aktuatora lub zasilania tu nie ma.

## 5. Czas i limity dowodu

Przykładowe ustawienia dowodu: timeout operacji 50 ms, maksymalna przerwa
serwisowania 100 ms, maksymalnie 8 odebranych ramek na krok, 16 rekordów w RX
atrapy. To wartości testowe, nie parametry SAC ani gwarancje hard-real-time.
`would_block` przy TX powoduje najwyżej jedną próbę w kolejnym kroku i podlega
ogólnemu deadline; nie ma pętli retry ze sleep.

Nadzór używa istniejącego DeadlineWatchdog. Deadline jest sprawdzany przed
pracą i przed zaakceptowaniem wyniku. Przekroczenie terminu przez błędny adapter
nie może zostać ukryte przez nadejście odpowiedzi.

Zegar ManualClock opisuje czas symulacji. Jego deklaracja ciągłości nie jest
implementacją zegara systemowego zachowującego czas podczas suspend.
Kontrakty wywołań są ograniczone, ale WCET i gwarancje planisty OS nie były mierzone.

## 6. Protokół atrapy i jego ograniczenia

Dowód sample wysyła na syntetycznym ID 0x600 osiem bajtów korelacji operacji;
syntetyczny partner odpowiada na 0x601. To nie echo własnego TX do ingress CORE.
Odpowiedź musi mieć właściwy format, długość, domenę czasu, korelację i czas
nie wcześniejszy od rozpoczęcia bieżącego wysłania. Starsza odpowiedź po timeout
lub anulowaniu nie kończy nowej operacji. Kolejki są czyszczone przy zamknięciu.

Żaden pakiet ani publiczny klient nie wybiera ID/payloadu. Parametry pochodzą
z zaufanej kompozycji programu. Ten protokół jest dowodem kontraktu, nie usługą
produkcyjną ani uniwersalnym sposobem korelacji UDS. Rzeczywiste protokoły wymagają
własnej obsługi spóźnionych odpowiedzi i osobnych testów.

## 7. Uruchomienie bez GUI i sprzętu

```sh
cmake -S . -B build/runtime-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/runtime-debug --config Debug --parallel 2
ctest --test-dir build/runtime-debug -C Debug --output-on-failure --no-tests=error
./build/runtime-debug/apps/runtime_mock/ecu_runtime_mock packages/examples/bench_mock
```

MSVC: program znajduje się w `apps/runtime_mock/Debug/ecu_runtime_mock.exe` pod
katalogiem buildu. CTest sam rozwiązuje ścieżkę konfiguracji.

Program emituje JSON z przyjęciem operacji, trzema odczytami w tej samej sesji,
timeoutem, bus-off, recovery, nowym connect oraz anulowaniem przy shutdown.
Nie wymaga sudo, sieci, CAN ani podłączonego urządzenia. Nie instaluje usługi.
Test CLI zmienia ID i nazwy operacji pakietu na dysku i powtarza dowód bez rebuild.

`ECU_APP_BUILD_MOCK_PROOFS=OFF` wyłącza program demonstracyjny. Przy wyłączonych
również testach host/runtime nie zależą od `support/testing` ani Pythona.
Dotychczasowy `ecu_app_host` pozostaje krótkim CLI katalogu z usługami echo/fail;
nie udaje serwisu sprzętowego.

## 8. Co dalej

Walidacja: [wyniki runtime](validation/runtime-mock-2026-10-10.md).
Następna praca w kroku 2: rzeczywisty executor/clock i platformowy adapter CAN,
przetestowane offline zanim zostanie uzgodniony zakres fizyczny. Dalej rzeczywista
usługa/profil SAC, a nie podmiana atrapy na sprzęt z syntetycznymi ramkami.
Nie wdrożono HTTP, WebGUI, izolacji procesowej dodatków ani podpisanego instalatora.
Nie dokonano merge ani zmiany istniejących usług CM5.
