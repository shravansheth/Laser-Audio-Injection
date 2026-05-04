# Laser Audio Injection — Display Test

Nokia 5110 shows "Lasec" on boot. Audio loaded from firmware flash, streams via I2S PDM.

See [wiring.md](wiring.md) for connections.

## Build

WAV must be 8-bit unsigned PCM, mono, 16 kHz. Set `board_build.embed_files = cmd1.wav` in platformio.ini.

```bash
pio run -t upload
```
