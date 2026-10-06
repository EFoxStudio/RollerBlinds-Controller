# BLIND//SYS – Mój sterownik rolet na ESP32-C3

Mój własny projekt sterownika do żaluzji/rolet oparty na małej płytce **ESP32-C3 Super Mini**. Całość sterowana jest przez stronę WWW, którą ESP serwuje bezpośrednio z pamięci Flash (żeby nie zapychać pamięci RAM). System pozwala na sterowanie ręczne, ustawianie czasów i harmonogramów, a nawet na automatyczne podnoszenie i opuszczanie rolet o wschodzie i zachodzie słońca.

---

## Co potrafi ten układ?

* **Ręczne sterowanie z telefonu/komputera:**
  * Można przytrzymać przycisk, żeby roleta jechała tylko wtedy, gdy trzymamy palec na ekranie (zabezpieczenie, żeby silnik nie jechał bez nadzoru).
  * Opcja "zjedź do końca" lub "podnieś do końca" jednym kliknięciem.
* **Automatyka i harmonogram:**
  * Ustawianie konkretnych godzin podnoszenia i opuszczania.
  * Opcja otwierania/zamykania o wschodzie i zachodzie słońca (godziny liczą się same dla współrzędnych Katowic).
  * Jednorazowe opóźnienie (np. "zjedź za 30 minut").
* **Dokładny czas przejazdu:**
  * Osobo ustawiany czas podnoszenia i opuszczania (z dokładnością do ułamka sekundy).
* **Automatyczny czas z sieci:**
  * Pobiera aktualny czas z serwerów NTP i sam ogarnia zmianę czasu z zimowego na letni.
* **Fajny interfejs WWW:**
  * Zrobiony w ciemnym motywie retro/cyberpunk.
  * Tryb jasny i ciemny (może się też sam przełączać dzień/noc).
  * Podgląd ostatnich logów i zdarzeń w panelu.

---

## Sprzęt i podłączenie pinów

* **Płytka:** ESP32-C3 Super Mini
* **Wykonanie:** Podwójny moduł przekaźnikowy do sterowania silnikiem 230V (kierunek góra / dół).

### Podłączenie do ESP32-C3:

* **Przekaźnik GÓRA:** GPIO 20
* **Przekaźnik DÓŁ:** GPIO 10
* **Dioda statusowa:** GPIO 8 (wbudowana dioda na płytce)

> **Ważne przy programowaniu:** GPIO 20 na tej płytce jest też połączony z linią RX od portu szeregowego. Żeby wgranie kodu i przekaźnik działały bez problemu, w Arduino IDE trzeba koniecznie włączyć opcję **USB CDC On Boot**.
