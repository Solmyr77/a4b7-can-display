# CANBUS Kijelző - Audi A4 B7

## 🚗 Projekt célja
Valós idejű kijelző rendszer Audi A4 B7-hez, ESP32 + MCP2515-M alapokon, amely CAN buszról olvas motordiagnosztikai adatokat és extra szenzorokat is kezel.

---

## 📚 Alkatrészek

| Eszköz | Funkció |
|--------|---------|
| **ESP32** | Központi vezérlő, CAN kiolvasás, PWM, kijelző |
| **MCP2515-M** | CAN vezérlő + TJA1050 transzceiver (SPI) |
| **150 Ω ellenállás** | Feszültség-osztó az olajnyomás szenzorhoz |
| **1k + 2k Ω ellenállás** | MISO vonal feszültség osztáshoz (5V → 3.3V) |
| **Step-down modul (STDN-3A24-ADJ)** | 12V → 5V táptápegység az ESP32-hez |
| **Kondenzátor** | Tápfeszültség stabilizáláshoz |
| **Piezo buzzer (SFN-12055PA6.5)** | Hangjelzés figyelmeztetés esetén |
| **RGB LED (common cathode)** | Vizuális állapotjelző |
| **DEPO Racing olajnyomás küldő** | Extra, nem gyári szenzor |

---

## 📊 Elérhető CAN adatok (OBD2-n keresztül, 500 kbps)

### Motor / hajtáslánc
- **Turbónyomás (MAP)** - ID `0x3C0` vagy `0x280`, 2 byte (pl. mbar)
- **Hűtőfolyadék hőmérséklet** - ID `0x280`, 1 byte
- **IAT (Intake Air Temp)** - ID `0x3C0`, 1 byte
- **Akkumulátor feszültség** - ID `0x288` vagy hasonló
- **Tempomat beállított sebesség** - ❌ Nem standard; extended ID-ben lehet, kísérleti

### Kapcsolók / állapotok
- **Fékpedál, kuplung** - ID `0x5A0`, 1 bites jelzők
- **Glow plug állapot** - ID `0x3E0` vagy `0x288`

---

## ✨ Extra (saját szenzorok)

### Olajnyomás (DEPO 3–160 Ω küldő)
- Feszültség-osztóval mérve ESP32 ADC bemenetre (pl. 150 Ω + belső ellenállás)
- Ellenállás → nyomás táblázat vagy lineáris átkonvertálás

### Olajhőmérséklet
- ❌ BLB motorban nincs gyári szenzor

---

## 🎮 Kimenetek / Jelzők

| Jelző          | Működés                       |
|------------------|----------------------------------|
| **Piezo buzzer** | Kritikus figyelmeztetés (olaj, hőmérséklet, boost) |
| **RGB LED**      | Állapotjelzés (zöld = OK, piros = hiba, sárga = figyelem) |
| **OLED/TFT kijelző** | Adatok megjelenítése valós időben |

---

## 🌐 Tervezett CAN ID-k (saját adatokhoz)

| Cél              | CAN ID | Megjegyzés |
|------------------|--------|-------------|
| Olajnyomás        | `0x600` | Egyedi keret |
| Olajhőmérséklet   | `0x601` | Egyedi keret |

---

## ⚒️ Fejlesztési lehetőségek
- OLED kijelzőn menü, adatok lapozása
- Logging SD kártyára
- Wi-Fi alapú dashboard (pl. telefonos megjelenítés)
- Figyelmeztetések naplózása

---

## 📆 Projekt állapota
- [x] Hardver kiválasztva
- [ ] CAN ID dekódolás
- [ ] Nyomás és hőmérséklet táblázatok betöltése
- [ ] Kijelző / LED vezérlés
- [ ] Figyelmeztetés logika
- [ ] Teszt autóban
