# Laser Audio Injection — Display Test

Nokia 5110 shows "Lasec" on boot. Audio loaded from firmware flash, streams via I2S PDM.

See [wiring.md](wiring.md) for connections.

---

## Preparing audio

Start with a WAV recording of the command you want to inject, then process it in Audacity:

**Format:**
1. Tracks → Stereo to Mono (if stereo)
2. Set the project rate to **16000 Hz** (bottom-left dropdown)

**Noise reduction pipeline** — run these in order:
1. Select a short clip of background silence → Effect → Noise Reduction → *Get Noise Profile*
2. Select all (Ctrl+A), then apply each of these in sequence:
   - Noise Reduction
   - Compressor
   - Normalize
   - Equalization — apply a Bass Boost curve, then a Treble Boost curve
   - Hard Limiter at −4 dB
   - Amplify — go in 2 dB increments until the signal is as loud as possible without clipping

**Export:** File → Export Audio → WAV, Unsigned 8-bit PCM. Save as `cmd1.wav` in the project root.

Set `board_build.embed_files = cmd1.wav` in platformio.ini and rebuild.

---

## Build

```bash
pio run -t upload
```
