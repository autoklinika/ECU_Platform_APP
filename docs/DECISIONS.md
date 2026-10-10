# Rejestr decyzji z rozmowy

Data bazowa: 2026-10-10. Rejestr zachowuje ostateczne ustalenia, a nie wszystkie
kolejne propozycje jako równolegle obowiązujące wymagania.

## D01 — Nowe repozytorium, kontrolowany import

**Ustalono:** cała nowa aplikacja i dokumentacja powstają w `ECU_Platform_APP`.
Przejęty fundament to CORE V2 z niezbędnymi testami i kontrolą architektury.
Nie rozwijamy hosta na wcześniej założonej gałęzi w V2. Nie nadpisujemy V3.

**Powód:** osobna, czytelna baza bez historycznych usług i zależności builda.
Zachowujemy pochodzenie kodu, zamiast przypisywać nowemu projektowi dawne PASS-y.

## D02 — V1 jako wzorzec funkcjonalny, nie błędny produkt do zapomnienia

**Ustalono:** zachować wygodę, trwałą sesję, prosty przepływ pracy oraz moduły DUT.
Nie kopiować VirtualCockpit ani transportu V1 do nowego CORE.

**Powód:** V1 działała dobrze użytkowo. Krytyczny błąd stanowiło zależne od GUI
wykonanie części operacji, nie sama obecność Qt. Dokumenty incydentów i kod
odniesienia wymieniono w [pochodzeniu](PROVENANCE.md).

## D03 — Mały host i instalowalne aplikacje

**Ustalono:** modularny host z rozszerzeniami, wspólny zaufany runtime i
instalowalne pakiety. Brak mikroserwisu dla każdego ECU, odczytu czy funkcji.

**Odrzucono:** rozrastający się program z kolejnymi `if OEM`, dowolne biblioteki
natywne uznawane za bezpiecznie odizolowane, rozproszoną orkiestrację na jednym
stanowisku i rozbudowane SDK zaprojektowane bez działającego przykładu.

## D04 — Niezależność przez kontrakty

**Ustalono:** moduł nie zna implementacji sąsiada. Rozszerzenie już dostępnych
możliwości nie wymaga edycji CORE, hosta, innych modułów ani menu.

**Doprecyzowanie:** nie oznacza to całkowitej niewiedzy o stanie stanowiska.
Moduł otrzymuje jawne capability, ograniczenia, wyniki i stan przez SDK.
Wspólne zasoby potrzebują koordynacji; izolacja nie usuwa fizyki CPU i magistrali.

## D05 — Jedna własność CAN i sesji

**Ustalono:** CanBusRuntime jest wspólnym właścicielem kanału. Profile, skaner,
protokoły i GUI nie otwierają go niezależnie. Trwała sesja nie kończy się przy
zmianie ekranu albo zakończeniu pojedynczego HTTP requestu.

**Odrzucono:** otwieranie i zamykanie stosu przy każdym parametrze, pliki jako
bieżący kanał komunikacyjny i stan UP/DOWN jako mutex.

## D06 — Specjalizacja bez dwóch konkurencyjnych platform

**Ustalono:** diagnostyka i cykliczne sterowanie są składanymi możliwościami.
Aktuator nie musi używać UDS; ECU może wymagać własnego CAN i diagnostyki razem.

**Wycofano jako obowiązek:** dwa rozłączne, uniwersalne Diagnostic/Actuator Engine
z osobnymi sesjami i arbitrami. Nazwy te mogą opisywać grupy usług, nie narzucać
osobnych procesów. Istniejących mechanizmów CORE nie implementujemy ponownie.

## D07 — Cała operacja niezależna od GUI

**Ustalono:** poza GUI działają nie tylko TX, ale także rampa, RX, pomiary,
watchdogi, stabilizacja i wszystkie kroki maszyny stanów testu.

**Powód:** osobny nadajnik ostatniej wartości nie rozwiązał incydentu EGR,
gdy logika testu nadal zależała od timerów i cache'u odświeżanego przez Qt.
Testy obciążenia GUI muszą mierzyć również sekwencję i jakość pomiarów.

## D08 — WebGUI jako klient

**Ustalono:** głównym interfejsem będzie modułowe WebGUI z powłoką, standardowymi
komponentami i widokami pakietów. Backend może pracować bez przeglądarki.

**Doprecyzowano:** granica HTTP sama nie gwarantuje prawidłowej architektury.
Timer JavaScript nie może sterować kolejnymi krokami testu. PWA jest opcją,
nie koniecznym etapem. Nie wprowadzamy Qt WebEngine tylko jako opakowania.
Framework WebGUI i biblioteka HTTP pozostają do wyboru na etapie implementacji.

## D09 — Pakiety o różnym poziomie zaufania

**Ustalono:** pakiet może dostarczać więcej niż dane. Logika aplikacji może
wymagać własnego kodu. Kod niekrytyczny izolujemy, jeśli uzasadnia to ryzyko;
kod w torze krytycznym jest zaufanym rozszerzeniem platformy z osobnym gate'em.

**Odrzucono:** twierdzenie, że podpisany `.so`/`.dll` nie może uszkodzić hosta,
lub że dowolny algorytm da się bezpiecznie zastąpić JSON-em.

## D10 — Instalacja nie oznacza hot-swapu aktywnego sterowania

**Ustalono:** walidacja, zależności, zgodność, transakcyjna aktywacja i rollback.
Zmiana aktywnego modułu czeka na bezpieczny koniec sesji. Zachowujemy zamrażaną
topologię CORE i wymagania czasu życia callbacków.

**Powód:** model „nowej appki” dotyczy ergonomii instalacji i niezależności kodu,
nie wymiany sterownika podczas wykonywania nieodwracalnej czynności.

## D11 — CORE rozwijalny, nie modyfikowany doraźnie

**Ustalono:** brakujące rzeczy najpierw trafiają do modułu, usługi, adaptera lub
SDK. Uzasadniony błąd albo ograniczenie ogólnego fundamentu może wymagać rewizji
CORE, świadomej zgodności, pełnej regresji i zgody właściciela.

**Wycofano jako zbyt kategoryczne:** „nowa aplikacja nigdy nie zmienia CORE”.
Dzisiejszy CORE nie musi znać wszystkich przyszłych możliwości. Nowa aplikacja
może wymagać aktualizacji platformy lub nowego sprzętu.

## D12 — Multiplatformowość i bezpieczeństwo

**Ustalono:** niezależność od Linux/CM5 jest własnością aplikacji i SDK, nie tylko
CORE. Sprzęt ma wymienne adaptery. Rzeczywiste wsparcie każdej platformy wymaga
kompilacji i testów; nie deklarujemy go wyłącznie na podstawie nagłówków.

**Ustalono:** błąd raportu nie steruje DUT; błąd aktywnego sterowania prowadzi do
jego zdefiniowanej reakcji bezpiecznej. Awaria hosta/OS może wymagać niezależnego
nadzoru sprzętowego. Stop ruchu i odłączenie zasilania nie są uniwersalnie tym samym.

## D13 — Prototyp lokalny a uprawnienia

**Ustalono wcześniej przez właściciela:** lokalny prototyp nie wymaga ręcznego
tokena. Nie rozszerza to uprawnień GUI i nie upoważnia do wystawienia API bez
kontroli w sieci. Polityka zdalnego dostępu i operacji ryzykownych pozostaje osobna.

## D14 — Workflow i dowody

**Ustalono:** małe kompletne kroki, najpierw działający dowód bez sprzętu,
następnie rzeczywiste przypadki. Nie mnożymy agentów, usług i skryptów wdrożeniowych.
`main` tylko za zgodą na konkretny changeset. Testy fizyczne i operacje ryzykowne
nie wynikają automatycznie ze zgody na kodowanie. CORE przejmujemy; pozostałe
komponenty są nową implementacją. Dokumentacja zawsze rozdziela plan i stan kodu.
