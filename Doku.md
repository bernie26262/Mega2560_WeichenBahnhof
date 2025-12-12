# Mega1 – Sensoren, Weichen, Fahrstraßen, Bahnhöfe

## 1. Schaltgleise (SensorHub, Indizes 0–10)

| Name | Index | Pin | Zweck                       |
|------|-------|-----|----------------------------|
| S0   | 0     | 22  | Fahrstraße (Trigger FS0)   |
| S1   | 1     | 28  | Fahrstraße (frei)          |
| S2   | 2     | 23  | Bhf 0/1 Einfahrt, FS1      |
| S3   | 3     | 33  | Fahrstraße (Reset FS0)     |
| S4   | 4     | 24  | Fahrstraße (FS2)           |
| S5   | 5     | 32  | Fahrstraße (frei)          |
| S6   | 6     | 25  | Fahrstraße (Reset FS0)     |
| S7   | 7     | 31  | Fahrstraße (FS3)           |
| S8   | 8     | 27  | Bhf 2/3 Einfahrt, FS4      |
| S9   | 9     | 35  | Fahrstraße (frei)          |
| S10  | 10    | 34  | Fahrstraße (Reset FS4)     |

Diese 11 Kontakte werden im `SensorHub` verwaltet und als `kontaktBits` (Bit 0–10) im Payload übertragen.

---

## 2. Timerstart-Sensoren (pro Bahnhof)

| Bhf | Name | Index | Pin | Zweck             |
|-----|------|-------|-----|-------------------|
| 0   | S19  | 19    | A8  | Timerstart Bhf 0  |
| 1   | S18  | 18    | A9  | Timerstart Bhf 1  |
| 2   | S22  | 22    | A10 | Timerstart Bhf 2  |
| 3   | S23  | 23    | A11 | Timerstart Bhf 3  |

Zuordnung im Code:

- Einfahrt Bhf 0 → S2 (Index 2)
- Einfahrt Bhf 1 → S2 (Index 2)
- Einfahrt Bhf 2 → S8 (Index 8)
- Einfahrt Bhf 3 → S8 (Index 8)

---

## 3. Weichen (WeichenHub)

| Weiche | Gerade | Abzweig | Reduktions-Pin | Rückmelde-Pin | Reduktion |
|--------|--------|---------|----------------|---------------|-----------|
| W0     | 5      | 6       | –              | 38            | nein      |
| W1     | 8      | 9       | –              | 39            | nein      |
| W2     | 10     | 11      | –              | 40            | nein      |
| W3     | 12     | 13      | –              | 41            | nein      |
| W4     | 14     | 15      | –              | 42            | nein      |
| W5     | 16     | 17      | –              | 43            | nein      |
| W6     | 18     | 19      | A12            | 44            | ja        |
| W7     | 2      | 3       | A13            | 45            | ja        |
| W8     | A0     | A1      | A14            | 46            | ja        |
| W9     | A2     | A3      | A15            | 47            | ja        |
| W10    | A4     | A5      | 50             | 48            | ja        |
| W11    | A6     | A7      | 50             | 49            | ja        |

W10 und W11 teilen sich Reduktions-Pin **50**.

---

## 4. Fahrstraßen (Fahrstrassen-Modul)

| FS | Trigger-Sensor | Reset-Sensor(en) | Bemerkung            |
|----|----------------|------------------|----------------------|
| 0  | S0 (Index 0)   | S3, S6           | definierbar          |
| 1  | S2 (Index 2)   | –                | Einfahrt Bhf 0/1     |
| 2  | S4 (Index 4)   | –                | definierbar          |
| 3  | S7 (Index 7)   | –                | definierbar          |
| 4  | S8 (Index 8)   | S10              | Einfahrt Bhf 2/3     |

`Fahrstrassen.activeRoute()` liefert den Index der aktiven Fahrstraße oder -1.

---

## 5. Modi

- **MANUELL (0)**: direkte Steuerung (z.B. durch Tasten / ESP-Kommandos)
- **AUTOMATIK (1)**: automatischer Bahnhofs-/Fahrstraßenbetrieb

Im Payload wird der Modus als `uint8_t modus` übertragen.

---

## 6. Mega1 → ESP Payload

```c
struct Mega1StatusPayload
{
    uint16_t bootId;
    uint16_t kontaktBits;  // Schaltgleise S0–S10
    uint16_t weichenBits;  // W0–W11 (1 = Gerade, 0 = Abzweig)
    uint8_t  modus;        // 0 = MANUELL, 1 = AUTOMATIK
    int8_t   activeRoute;  // -1 = keine Fahrstraße aktiv
    uint8_t  errorFlags;   // Fehler vom BahnhofController
};
