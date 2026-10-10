# Instrukcje pracy w ECU Platform APP

## Granice projektu

- Właściciel podejmuje decyzje projektowe i zatwierdza produkcyjne scalenia.
- Pracuj tylko w tym repozytorium. Nie łącz go z AI Platform, WVC,
  ECU_Platform_V3 ani firmware Power Managera bez osobnej dyspozycji.
- Zatwierdzony import: wyłącznie CORE V2, jego testy i kontrola architektury
  wskazane w `provenance/core_v2_import.json`.
- V1/V2 mogą być czytane jako źródła wiedzy i dowodów pochodzenia. Nie przenoś
  ich usług, GUI, agentów, skryptów wdrożeniowych ani historycznych PASS-ów.

## Nieprzekraczalne zasady

1. Nie zapisuj ani nie scalaj do `main` bez jawnej zgody na konkretny changeset.
2. Zachowaj publiczne kontrakty i odrębność modułów; nie dodawaj wyjątków OEM do hosta.
3. GUI wyraża zamiar; backend sprawdza uprawnienia, stan i warunki wykonania.
4. Żadne timery GUI/HTTP/IPC nie wyznaczają kolejnych kroków sterowania.
5. Jedna autorytatywna ścieżka dostępu do każdego fizycznego kanału CAN.
6. CORE nie otrzymuje zależności od OS, GUI, hosta, SDK pakietów ani urządzeń OEM.
7. Zmiana CORE jest dozwolona tylko po wykazaniu błędu lub ograniczenia ogólnego
   kontraktu i zatwierdzeniu osobnej rewizji. Nie regeneruj manifestu importu,
   aby tylko uciszyć test integralności. Czytaj `docs/CORE_EVOLUTION.md`.
8. Zachowaj multiplatformowość całej warstwy aplikacyjnej, nie tylko CORE.
9. Testy i build nie dotykają CAN, RS485, zasilania, usług ani aktywnego kiosku.
10. Zgoda na implementację nie upoważnia do DTC clear, ruchu EGR/VGT,
    zmiany zasilania, flashowania lub wdrożenia produkcyjnego.

## Codzienna praca

Sprawdź gałąź i stan Git. Czytaj bieżące dokumenty, a nie tylko starszą rozmowę.
Zmieniaj możliwie jeden spójny obszar. Uruchom testy związane ze zmianą i pełną
regresję przed przedstawieniem wydania. Nie dodawaj kolejnego serwisu, worktree
ani narzędzia operacyjnego dla każdej drobnej poprawki.

W dokumentach i odpowiedziach używaj polskiego. W kodzie zachowaj spójny język
API. Raportuj osobno: założenia, implementację, testy lokalne, CI i testy fizyczne.
Nie utożsamiaj zielonego CI z certyfikacją protokołu, bezpieczeństwem funkcjonalnym
ani gotowością sprzętową. Nie obiecuj pracy asynchronicznej między rozmowami.

## Minimum kontroli

```sh
python3 tools/verify_core_import.py
python3 scripts/check_core_v2_architecture.py src/core_v2
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --config Debug --parallel 2
ctest --test-dir build/debug -C Debug --output-on-failure --no-tests=error
```

Windows: użyj `python` zamiast `python3`, jeśli tak nazywa się interpreter.
Dokładny zakres aktualnego fundamentu: `docs/VALIDATION.md`.
