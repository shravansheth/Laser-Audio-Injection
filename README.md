# Laser Audio Injection

Amplitude-modulates a laser with audio via I2S PDM on the ESP32-C6. A MOSFET switches
the laser at ~960 kHz — time-averaged intensity encodes the audio signal.

Audio is embedded in firmware flash and loops continuously.

See [wiring.md](wiring.md) for connections.

## Build

WAV must be 8-bit unsigned PCM, mono, 16 kHz. Set `board_build.embed_files = cmd1.wav` in platformio.ini.

```bash
pio run -t upload
```
