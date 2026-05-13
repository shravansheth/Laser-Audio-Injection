# Laser Audio Injection

SD card WAV playback via I2S PDM laser modulation. Scans the card for WAV files and
plays the first one found on loop.

See [wiring.md](wiring.md) for connections.

## Build

WAV files must be 8-bit unsigned PCM, mono, 16 kHz. Copy to the root of the SD card.

```bash
pio run -t upload
```
