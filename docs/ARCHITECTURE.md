# Architektura ECU Platform APP

Status: **ustalony kierunek; host i moduły jeszcze niezaimplementowane**.
Źródło: ustalenia właściciela w rozmowie z 2026-10-10 oraz wnioski z V1/V2.

## 1. Co budujemy

Zakres produktu: wyłącznie TRUCK / AGRI / OHV, z jednym DUT jako domyślną
topologią stanowiska. Multiplatformowość OS i wymienne peryferia są wymaganiami
podstawowymi. Nie rozszerzamy zakresu o samochody osobowe ani inne projekty.

Mały host platformy z instalowalnymi aplikacjami laboratoryjnymi. Wspólny zaufany
runtime C++ korzysta z CORE V2. Niezależne WebGUI jest klientem. Osobne procesy
pojawiają się wybiórczo, gdy wymagają tego granice awarii lub ciężka praca.
Nie tworzymy mikroserwisu dla każdego ECU, parametru czy operacji.

Celem operatora jest: zainstalować pakiet, zobaczyć aplikację w katalogu,
uruchomić ją na wolnym stanowisku. Rozszerzenie istniejących możliwości nie
powinno wymagać edycji hosta, menu ani innych aplikacji. Nowa klasa możliwości
może wymagać aktualizacji SDK/runtime, a uzasadnione ograniczenie fundamentu —
rewizji CORE. Nie obiecujemy, że dowolne przyszłe urządzenie będzie obsługiwane
bez zmiany platformy lub sprzętu.

V1 jest wzorcem sprawności pracy: trwały silnik, kolejka poleceń, migawka danych
oraz moduły urządzeń. Nie kopiujemy jej zależności od Qt, sterowania magistralą
z QML ani maszyn stanów testów w wątku GUI. Z V2 pozostaje kontrolowany CORE,
nie proces uruchamiany na każdy odczyt i nie komunikacja przez pliki wynikowe.

## 2. Trzy niezależności

**Implementacyjna:** moduł korzysta z publicznego kontraktu. Nie importuje klas
innego modułu i nie zna jego struktur danych, GUI ani kodu.

**Czasowa:** odbiór CAN, sekwencje, generowanie wartości zadanej, harmonogramy,
pomiary czasu i nadzór są poza GUI, HTTP, eksportem i zapisem raportów.

**Awarii:** dodatek uruchomiony poza runtime nie może bezpośrednio naruszyć jego
pamięci ani sterować sprzętem. Osobny proces sam w sobie nie jest pełnym sandboxem;
potrzebne są ograniczenia uprawnień, IPC i zasobów. Biblioteka natywna w procesie
hosta nie daje izolacji pamięci. Pozostaje zaufaną częścią platformy.

Wspólny CPU, RAM, system i CAN oznaczają, że absolutnego braku wpływu nie da się
zagwarantować samym diagramem. Wymagamy limitów, pomiarów i zachowania awaryjnego.

## 3. Części systemu i zależności

```text
WebGUI Shell + widoki zainstalowanych aplikacji
                    |
          wersjonowane publiczne API
                    |
Host platformy C++ (jeden proces backendowy)
  katalog pakietów / zgodność / aktywacja
  sesja stanowiska / operacje / przydział zasobów
  migawki / zdarzenia / wyniki
  zaufane usługi wykonawcze i nadzór
                    |
CORE V2 — czas, CAN Runtime, protokoły, zasoby, safety
                    |
Adaptery platformy — OS, konkretny CAN, pomiary, zasilanie
                    |
                   DUT

Opcjonalne procesy pomocnicze dodatków
  własne algorytmy niekrytyczne / raporty / analizy
  komunikacja z hostem tylko przez ograniczony kontrakt
```

Jest to widok odpowiedzialności, nie kompletny graf wywołań. Adapter sprzętowy
implementuje kontrakt od strony platformy; CORE nie zależy od jego implementacji.
SDK modułu nie jest publicznym eksportem wszystkich prywatnych klas CORE.
Granice CMake mają w przyszłości wymuszać kierunki zależności.

| Warstwa | Zna | Nie zna / nie wykonuje |
|---|---|---|
| WebGUI | operacje, capability, stan i komponenty prezentacji | ramek, rytmu TX, sudo, portów sprzętowych |
| Moduł DUT | procedury i semantykę własnego urządzenia, SDK | implementacji innych DUT, fizycznego sterownika CAN |
| Host | lifecycle, zgodność, zasoby, politykę operacji | instrukcji `if SAC` / `if EGR` dla logiki OEM |
| Usługi runtime | generyczne wykonanie i nadzór | nawigacji i stanu widoku |
| CORE | własne przenośne kontrakty | GUI, hosta, bibliotek OEM, systemu pakietów |
| Adapter | konkretny sprzęt i system | znaczenia DTC lub ekranów aplikacji |

## 4. Jedna sesja i jeden właściciel CAN

Jeden fizyczny kanał ma jedną autorytatywną ścieżkę odbioru i nadawania przez
`CanBusRuntime`. Moduły nie otwierają SocketCAN/J2534 ani nie zmieniają bitrate.
Host odpowiada za przydział zasobów i kompozycję, a CORE za swoje mechanizmy.
Nie dodajemy drugiego ogólnego arbitra, schedulera, EventBus ani ResourceManagera
bez wykazania braku w istniejących kontraktach.

Sesja żyje dłużej niż ekran i pojedynczy request HTTP. Dla SAC: wykrycie prędkości
zgodnie z profilem, identyfikacja, utrzymywanie komunikacji i pomiarów, operacje
DTC, a następnie jawne rozłączenie. Nawigacja nie uruchamia identyfikacji ponownie
ani nie wykonuje CAN DOWN/UP. Dobór metody wykrywania dla milczącego DUT jest
polityką profilu i bezpieczeństwa, nie uniwersalną obietnicą autodetekcji.

Własność logiczna w procesie nie blokuje sama obcego programu systemowego.
Przyszłe wdrożenie musi egzekwować uprawnienia urządzeń i konfiguracji, a także
obsłużyć obce zmiany lub utratę adaptera. Samo UP/DOWN nie jest blokadą.

Obiekty CORE pozostają single-executor. Jeden wskazany kontekst wykonuje ich
mutujące operacje; inne wątki przekazują ograniczone komendy i odbierają kopie
stanu. Nie wolno wywoływać tego samego CanBusRuntime równocześnie z RX, HTTP,
workerem aplikacji i timerem TX. Dokładny model wątków hosta oraz sterowników
będzie dobrany na podstawie pomiarów, bez zmiany tego niezmiennika.

CORE zamraża topologię przed pracą. Katalog zainstalowanych aplikacji nie jest
topologią aktywnego CORE. Nową konfigurację budujemy pomiędzy sesjami; obiekty
sterownika i odbiorców żyją co najmniej tak długo jak runtime. Nie usuwamy sinka
podczas callbacku i nie odblokowujemy `freeze_configuration` dla hot-loadingu.

## 5. Możliwości składane, nie konkurencyjne silniki

Diagnostyka request-response, pasywny odbiór, cykliczne sterowanie oraz sekwencje
testowe są różnymi usługami wykonawczymi. Jeden DUT może potrzebować kilku naraz.
Nie wymuszamy UDS na aktuatorze i nie budujemy dwóch niezależnych światów ECU/EGR
z własnymi sesjami oraz własnymi właścicielami CAN.

Nazwy Diagnostic Engine i Actuator Engine mogą opisywać grupy funkcji, ale nie
wymuszają procesów ani duplikacji CORE. Protokół OEM, licznik, skala i znaczenie
pozycji należą do modułu; terminowość i egzekwowanie ograniczeń należą do zaufanego
toru wykonania. Ciężki raport nie działa w tym torze.

## 6. Komendy, stan i zdarzenia

GUI przesyła zamiar, np. połączenie, odczyt DTC albo uruchomienie nazwanego testu.
Backend niezależnie sprawdza schemat, uprawnienia, sesję, możliwości, zasoby oraz
warunki bezpieczeństwa. Nie ma ogólnego endpointu wykonującego dowolny shell,
payload, metodę C++ czy arbitralną ramkę przekazaną przez przeglądarkę.

Długie operacje mają identyfikator, identyfikator/generację sesji i stan.
Przyjęcie polecenia nie jest potwierdzeniem jego wykonania. Retries wymagają
jawnej idempotencji. Anulowanie oznacza żądanie bezpiecznego zakończenia, a nie
obietnicę cofnięcia już wykonanej czynności fizycznej. API nie utrzymuje całego
cyklu życia operacji w wielosekundowym POST uruchamiającym subprocess.

Kolejki są ograniczone. Przeciążenie ma jawny status. STOP i nadzór nie czekają
za nieograniczoną kolejką poleceń. Telemetria może być agregowana lub odrzucana
z licznikiem strat; wyniki i zdarzenia krytyczne wymagają oddzielnej polityki
trwałości. Jeden mechanizm nie musi zapewniać obu klas gwarancji.

Stan DUT jest autorytatywny w backendzie. API i GUI otrzymują snapshoty z wersją,
sekwencją oraz powiązaniem z sesją. Dane historyczne nie stają się live po wejściu
na ekran. Wartość pomiaru, jednostka, źródło, czas, ważność i przyczyna braku
pomiaru nie mogą zostać zastąpione syntetycznym zerem.

## 7. Czas, pomiary i utrata klienta

Praca diagnostyczna i cykliczna nie zależy od subskrypcji ekranu. Host może
ograniczać zbędny odczyt według polityki, lecz GUI nie staje się harmonogramem.
Maszyna stanów testu nie odczytuje cache'u kontrolera GUI — używa danych runtime
ze znacznikami czasu. Równy TX ostatniej wartości nie dowodzi terminowości rampy,
feedbacku ani przejść testu. To zasadnicza lekcja incydentów EGR w V1.

Do deadline'ów używamy uzgodnionej domeny monotonicznej, nie zegara ściennego.
Reakcja na suspend/resume, skok lub awarię zegara musi respektować kontrakt CORE.
Timestamp protokołu, czas odebrania przez host i czas wyświetlenia to różne rzeczy.
Wymagania synchronizacji mogą wymagać nowego sprzętu albo rozszerzenia platformy.

Odłączenie GUI nie niszczy automatycznie sesji. Zachowanie zależy od operacji:
niegroźny odczyt może się zakończyć; ręczne sterowanie wymaga terminowo odnawianego
uprawnienia i bezpiecznej reakcji po wygaśnięciu; test autonomiczny ma jawnie
określoną politykę kontynuacji lub przerwania. Nie wznawiamy starego celu po awarii.

GUI może oznaczyć brak połączenia z backendem i nieaktualność swojej prezentacji.
Nie wolno prezentować wiecznie świeżej wartości po zatrzymaniu strumienia, nawet
gdy ostatni backendowy snapshot był poprawny. To lokalna ochrona wyświetlania,
nie przejęcie decyzji o stanie sprzętu ani timingu przez przeglądarkę.

## 8. Pakiety i izolacja

[Kontrakt modułów](MODULE_CONTRACT.md) rozróżnia dane/procedury korzystające
z gotowych usług, własny kod dodatku oraz zaufane rozszerzenie toru krytycznego.
Nie zamieniamy JSON w nieograniczony język sterowania i nie wykonujemy instalacyjnego
skryptu pakietu z prawami do sprzętu. Podpis potwierdza pochodzenie, nie poprawność.

Awaria raportu powinna pozostać awarią raportu. Awaria aktywnego sterowania wymaga
bezpiecznego zakończenia tej operacji; izolacja nie oznacza kontynuowania ruchu
bez odpowiedzialnego modułu. Błąd natywnej wtyczki w procesie hosta może uszkodzić
cały host, dlatego taka wtyczka jest częścią zaufanej bazy i przechodzi szerszy gate.

Nie próbujemy osiągać wymagających deadline'ów wymianą pojedynczych kroków przez
IPC/HTTP. Usługi cykliczne i sekwencje wymagające terminowości wykonują się w
zaufanym runtime. Brakujący mechanizm rozszerzamy zgodnie z CORE_EVOLUTION,
nie przez obejście tej granicy.

## 9. WebGUI i API

WebGUI jest wybranym klientem: lokalny kiosk, możliwy późniejszy dostęp z innych
urządzeń, wspólna powłoka i widoki pakietów. Działa lokalnie bez Internetu, gdy
zasoby i backend są lokalne. PWA jest opcją, nie wymaganiem ani implementacją
w tej rewizji. Framework, biblioteka HTTP, format strumienia i porty nie są jeszcze
wybrane. HTTP + SSE jest kandydatem; WebSocket tylko przy uzasadnieniu.

Granica API pomaga w separacji, ale sama jej nie gwarantuje. WebGUI nie może
odtwarzać timera sterowania z QML w JavaScript. Pakiety widoków nie uzyskują
przez samo zainstalowanie uprawnień API; backend każdorazowo je egzekwuje.
Nieznany JavaScript we wspólnym origin może naruszyć powłokę, więc sposób izolacji
własnych widoków wymaga osobnej implementacji i testów.

Dla lokalnego prototypu właściciel dopuścił brak ręcznie wpisywanego tokena.
Nie oznacza to API dostępnego w sieci bez kontroli. Lokalny tryb wymaga loopback,
walidacji Host/Origin i ochrony przed żądaniami obcych stron, allowlisty operacji
oraz rozdzielenia uprawnień kiosku i backendu. Zdalny dostęp, aktualizacja
pakietów i operacje ryzykowne potrzebują osobnych polityk. Nie utrwalamy dawnego
wymagania tokena jako jedynego możliwego mechanizmu uwierzytelniania.

## 10. Sprzęt, bezpieczeństwo i zakres obietnic

Kontrakty logiczne mocy, zapłonu, wake i pomiarów nie wybierają konkretnego GPIO,
przekaźnika ani systemu. ACK polecenia przekaźnika nie jest pomiarem stanu wyjścia.
Brak sprzętu jest jawnie zgłaszany; platforma nie emuluje potwierdzenia.

Bezpieczny stan zależy od DUT. Nie zakładamy, że zawsze oznacza ramkę z zerem
albo natychmiastowe wyłączenie zasilania. Nieudany safe-stop nie jest sukcesem
rozłączenia. Przy zawieszeniu procesu lub OS nadzór programowy może nie wykonać
żadnej czynności; tam, gdzie wymaga tego analiza ryzyka, potrzebny jest niezależny
watchdog sprzętowy i fizyczne zatrzymanie awaryjne. Nie deklarujemy certyfikacji
bezpieczeństwa ani twardego czasu rzeczywistego na podstawie tej architektury.

## 11. Jak udowodnimy granice

Host bez GUI; nawigacja bez reconnectu; obciążenie renderera bez zmiany sekwencji
aktuatora; przepełnienie kolejki; odrzucenie starej generacji; awaria workera;
zniknięcie klienta; brak uprawnień do CAN; konflikty pakietów; brak wymaganych
capability; testy Windows/Linux; pomiary na docelowym sprzęcie.
Kryteria etapów są w [planie](ROADMAP.md). Żaden z tych przyszłych testów nie jest
zaliczony samym skopiowaniem CORE.
