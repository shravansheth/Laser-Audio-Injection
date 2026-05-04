# Wiring

Nokia 5110 display + laser. Audio is baked into the firmware — no SD card, no buttons.

## Connections

| XIAO Pin | GPIO | → |
|----------|------|---|
| D0 | GPIO0 | 220 Ω → MOSFET gate |
| D8 | GPIO19 | Display SCLK |
| D9 | GPIO20 | Display DC |
| D10 | GPIO18 | Display MOSI |
| — | GPIO21 | Display RST |
| — | GPIO22 | Display CS |
| 5V | — | Laser (+) |
| 3V3 | — | Display VCC, Display LED |
| GND | — | Common ground |

---

## Diagram

```
  XIAO ESP32-C6
  ┌──────────────────────┐
  │              5V(USB) ├────────────────────────────────────► Laser (+)
  │                  3V3 ├──┬─────────────────────────────────► Display VCC
  │                      │  └─────────────────────────────────► Display LED
  │                      │
  │              GPIO0   ├──[220Ω]──┬──► MOSFET Gate
  │                      │        [10kΩ]
  │                      │          │
  │             GPIO18   ├──────────┤── Display MOSI
  │             GPIO19   ├──────────┤── Display SCLK
  │             GPIO20   ├──────────┤── Display DC
  │             GPIO21   ├──────────┤── Display RST
  │             GPIO22   ├──────────┤── Display CS
  │                  GND ├──────────┴── GND bus
  └──────────────────────┘
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
| DC | GPIO20 |
| DN/MOSI | GPIO18 |
| SCLK | GPIO19 |
| LED | 3V3 (add 33 Ω if no onboard resistor) |

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
