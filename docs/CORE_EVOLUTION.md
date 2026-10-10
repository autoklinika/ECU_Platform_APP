# Rozwój platformy i CORE przy nowych wymaganiach aplikacji

Status: obowiązująca zasada architektoniczna. CORE jest stabilny, nie wiecznie
niezmienny. Liczba nowych funkcji aplikacji nie przesądza, która warstwa się zmieni.

## 1. Klasyfikacja brakującej możliwości

| Nowa potrzeba | Pierwsze miejsce realizacji |
|---|---|
| Parametry, DID/PGN, dekodowanie, procedura i ekran konkretnego DUT | Moduł aplikacji / profil |
| Wspólny algorytm charakterystyki lub eksport wyników | Biblioteka albo usługa platformy |
| Nowy miernik, adapter CAN lub Power Manager | Adapter sprzętowy i usługa platformy |
| Udostępnienie dodatkom już istniejącej możliwości | Publiczny SDK/API, z kontrolą uprawnień |
| Nowy mechanizm czasu, zasobów lub wykonania | Zaufany runtime; CORE tylko przy braku ogólnego kontraktu |
| Błąd fundamentu albo ograniczenie uniemożliwiające poprawne wykonanie | Osobna rewizja CORE |

Nie wszystko wspólne musi mieszkać w CORE. Nie każdy nowy protokół wymaga nowego
runtime magistrali. Z drugiej strony nie obiecujemy, że każdy przyszły transport
lub algorytm da się wyrazić przez dzisiejsze kontrakty.

## 2. Procedura decyzji

1. Opisz wymaganą funkcję i dowód braku: konkretny kontrakt, limit, zależność,
   czas lub warunek bezpieczeństwa. Oddziel potrzebę od preferowanej implementacji.
2. Sprawdź moduł, usługę, adapter i SDK. Zidentyfikuj możliwość kompozycji istniejących
   usług. Nie duplikuj schedulera, transportu ani mechanizmu zasobów dla jednego DUT.
3. Jeżeli potrzebna jest zmiana CORE, wykaż błąd lub ograniczenie ogólne. Nowa
   funkcja profilu sama w sobie nie jest takim uzasadnieniem.
4. Przygotuj decyzję właściciela: zmieniany kontrakt, alternatywy, wpływ na stare
   moduły, zasoby, platformy, bezpieczeństwo oraz politykę zgodności i migracji.
5. Implementuj w osobnym, czytelnym changesecie wraz z testem odtwarzającym potrzebę.
6. Wykonaj pełną regresję CORE i jego granic, Linux/Windows, sanitizery, testy
   dotychczasowych modułów oraz pomiary sprzętowe stosowne do zmiany.
7. Uaktualnij wersję kontraktu, manifest pochodzenia i listę odchyleń dopiero
   w zatwierdzonym review. Merge do `main` nadal wymaga osobnej zgody.

Testy normatywne, czasowe i bezpieczeństwa mają odpowiadać rzeczywistej zmianie.
Samo zielone CI nie zastępuje tych dowodów. Zmiana kompatybilna też może pogorszyć
latencję albo pamięć i wymaga stosownej regresji.

## 3. Przykład: aplikacja VGT z nowym pomiarem i testem

Własne ramki: semantyka OEM w module, wykonanie przez zatwierdzony tor CAN.
Pomiar prądu: nowy adapter i usługa pomiaru, nie protokół każdego miernika w CORE.
Algorytm charakterystyki: moduł albo wspólna biblioteka analizy.

Jeśli test wymaga synchronizacji pomiaru i sterowania, której obecne kontrakty
nie zapewniają, projektujemy nową zdolność platformy. Może być potrzebne nowe
źródło czasu, rozszerzenie kontraktu lub sprzęt. Nie zastępujemy wymagań
nieudowodnionym timerem i nie obchodzimy właściciela CAN.

Przy braku sprzętu lub gwarancji funkcja jest niedostępna. Aplikacja nie otrzymuje
fałszywego potwierdzenia jakości ani cichego trybu o innych wymaganiach.

## 4. Stabilność API i wersjonowanie

Preferujemy dodawanie nowych możliwości bez zmiany znaczenia starych.
Capability i zgodna wersja API są sprawdzane przed aktywacją aplikacji.
Jeżeli zmiany znaczenia nie da się uniknąć, powstaje jawnie niezgodna wersja
kontraktu i plan migracji; stare aplikacje nie są uruchamiane z błędną interpretacją.

SDK ogranicza zależność od szczegółów CORE. Biblioteki C++ kompilowane z innym
ABI nie są automatycznie zgodne przez samą zgodność nagłówka. IPC wykorzystuje
jawny schemat danych. Identyfikator sesji, generacja, jednostki i wynik cleanupu
są częścią semantyki kontraktu, a nie szczegółem GUI.

Nie obiecujemy, że dowolna aplikacja zainstaluje się na każdej starej platformie.
Czytelne wymaganie aktualizacji SDK/runtime lub sprzętu jest poprawnym zachowaniem.
Aktualizacja odbywa się po bezpiecznym zakończeniu sesji i nie może po cichu
wyłączyć innych zainstalowanych aplikacji.

## 5. Pojemności i granice czasu

CORE używa ograniczonych zasobów i zamrożonej topologii. Jeżeli pakiet przekracza
limit, najpierw sprawdzamy filtrację, kompozycję i rzeczywistą potrzebę. Nie
zwiększamy bez kontroli tablic i budżetów dla każdego nowego DUT. Gdy limit
fundamentu jest niewystarczający, jego zmiana ma udokumentowany koszt pamięci,
czasu i wpływ na wspierany sprzęt.

Host sprawdza możliwości przed sesją. Błąd przydziału lub cleanupu ma jawny stan;
nie ukrywamy go przez retry, restart usługi ani odrzucenie odpowiedzi w GUI.

## 6. Ochrona kopii źródłowej

Aktualny import jest byte-exact i sprawdzany w CTest/CI. Planowana zmiana CORE
nie jest powodem do wyłączenia tego testu. Należy zachować bazowy manifest,
udokumentować odchylenia lub nową rewizję i rozszerzyć kontrolę integralności
w tym samym przeglądanym changesecie. Sama podmiana sum na bieżące bajty nie jest
procedurą aktualizacji.

Jeżeli niemal każdy kolejny DUT wymaga zmian CORE, analizujemy granice
architektury. Nie dokładamy kolejnych warunków OEM do fundamentu.
