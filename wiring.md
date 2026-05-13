# Wiring

Nokia 5110 display + SD card + two buttons. Display and SD share the SPI bus (MOSI/SCK) with separate CS lines.

## Connections

| XIAO Pin | GPIO | → |
|----------|------|---|
| D0 | GPIO0 | 220 Ω → MOSFET gate |
| D1 | GPIO1 | BTN_PLAY (other leg → GND) |
| D5 | GPIO23 | Display DC |
| D6 | GPIO16 | SD CS |
| D7 | GPIO17 | BTN_NEXT (other leg → GND) |
| D8 | GPIO19 | Display SCLK / SD SCK |
| D9 | GPIO20 | SD MISO |
| D10 | GPIO18 | Display MOSI / SD MOSI |
| — | GPIO21 | Display RST |
| — | GPIO22 | Display CS |
| 5V | — | SD VCC, Laser (+) |
| 3V3 | — | Display VCC, Display LED |
| GND | — | Common ground |

GPIO21 and GPIO22 are labeled on the board but not on the D0–D10 headers.

---

## Diagram

```
  XIAO ESP32-C6
  ┌───────────────────────┐
  │              5V (USB) ├──┬────────────────────────────────► SD VCC
  │                       │  └────────────────────────────────► Laser (+)
  │                   3V3 ├──┬────────────────────────────────► Display VCC
  │                       │  └────────────────────────────────► Display LED
  │                       │
  │               GPIO0   ├──[220Ω]──┬──► MOSFET Gate
  │                       │        [10kΩ]
  │                       │          │
  │               GPIO1   ├──────────┤── BTN_PLAY ──► GND
  │              GPIO16   ├──────────┤── SD CS
  │              GPIO17   ├──────────┤── BTN_NEXT ──► GND
  │              GPIO18   ├──────────┤── Display MOSI ──────► SD MOSI
  │              GPIO19   ├──────────┤── Display SCLK ──────► SD SCK
  │              GPIO20   ├──────────┤── SD MISO
  │              GPIO21   ├──────────┤── Display RST
  │              GPIO22   ├──────────┤── Display CS
  │              GPIO23   ├──────────┤── Display DC
  │                   GND ├──────────┴─────────────────────── GND bus
  └───────────────────────┘
                           MOSFET: Drain → Laser (–), Source → GND
```

---

## Nokia 5110 (8 pins)

| Pin | → |
|-----|---|
| VCC | 3V3 |
| GND | GND |
| SCE | GPIO22 |
| RST | GPIO21 |
| DC | GPIO23 |
| DN/MOSI | GPIO18 |
| SCLK | GPIO19 |
| LED | 3V3 (add 33 Ω if no onboard resistor) |

---

## SD Module

| Pin | → |
|-----|---|
| VCC | **5V** |
| GND | GND |
| CS | GPIO16 |
| MOSI | GPIO18 |
| SCK | GPIO19 |
| MISO | GPIO20 |

---

## MOSFET

```
GPIO0 ──[220Ω]──┬──► Gate
              [10kΩ]
                │
               GND        Drain ──► Laser (–)
                           Source ──► GND

5V ──► Laser (+)
```

Add a 33 Ω series resistor on Laser (+) if the module has no built-in current limiting.

---

## Buttons

Both use `INPUT_PULLUP` — no external resistor needed.

```
GPIO1  ──► BTN_PLAY ──► GND
GPIO17 ──► BTN_NEXT ──► GND
```
