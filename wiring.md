# Wiring

Just the laser. Audio is baked into the firmware — no display, no SD card.

## Connections

| XIAO Pin | GPIO | → |
|----------|------|---|
| D0 | GPIO0 | 220 Ω → MOSFET gate |
| D3 | GPIO5 | 220 Ω → power LED |
| 5V | — | Laser (+) |
| GND | — | Common ground |

---

## Diagram

```
  XIAO ESP32-C6
  ┌──────────────────┐
  │          5V(USB) ├────────────────────────────────────────► Laser (+)
  │                  │
  │          GPIO0   ├──[220Ω]──┬──► MOSFET Gate
  │                  │        [10kΩ]
  │                  │          │
  │          GPIO5   ├──[220Ω]──┤── LED (+) ──► LED ──► GND
  │                  │          │
  │              GND ├──────────┴──────────────────────────── GND bus
  └──────────────────┘
                       MOSFET: Drain → Laser (–), Source → GND
```

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

## Power LED

```
GPIO5 ──[220Ω]──► LED (+) ──► LED ──► GND
```

Turns on at the end of `setup()`.
