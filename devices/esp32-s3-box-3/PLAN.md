# HeyClawy on ESP32-S3-BOX-3 — Implementation Plan

## Goal
Talk to OpenClaw directly through the BOX-3 like a person. No Home Assistant in the voice loop.

## Architecture
```
ESP32-S3-BOX-3 (mic/speaker)
    ├── WebSocket → OpenClaw gateway (ws://server:18789) — conversation
    ├── HTTP POST → faster-whisper (http://server:5051) — speech-to-text
    └── HTTP GET  ← EdgeTTS (http://server:5050) — text-to-speech
```

## Phase 1: Server Side (PowerEdge at 192.168.1.179)

### 1a. Add EdgeTTS container
- Docker container: `travisvn/openai-edge-tts`
- Port 5050
- Add to `infrastructure/openclaw/docker-compose.yml` or its own compose file
- Test: `curl -X POST "http://192.168.1.179:5050/v1/audio/speech" -H "Content-Type: application/json" -d '{"model":"tts-1","voice":"alloy","input":"Hello"}' --output hello.mp3`

### 1b. Add faster-whisper HTTP endpoint
- Need an HTTP-based whisper service (not Wyoming protocol)
- Port 5051
- Options: `fedirz/faster-whisper-server` or similar
- Test with a WAV file POST

### 1c. Expose OpenClaw WebSocket through NGINX
- Add WebSocket proxy rule to `infrastructure/nginx/conf.d/production.conf` or `local.conf`
- Proxy `wss://openclaw.ops.markcheli.com/ws` → `ws://openclaw:18789`
- Or expose port 18789 directly on the host for LAN access

### 1d. Get OpenClaw API token
- Needed for HeyClawy device authentication
- Generate device key: `python tools/test_openclaw_auth.py`

## Phase 2: Port HeyClawy to ESP32-S3-BOX-3

HeyClawy currently supports: SenseCAP Watcher, M5StickC Plus2, Waveshare Audio Board.
The BOX-3 uses different hardware that needs a new board support package.

### Hardware differences to handle
| Component | HeyClawy (existing) | BOX-3 |
|-----------|-------------------|-------|
| Audio codec | Varies by board | ES7210 (mic) + ES8311 (speaker) via I2C |
| I2S bus | Board-specific | Shared: LRCLK=GPIO45, BCLK=GPIO17, MCLK=GPIO2, DIN=GPIO16, DOUT=GPIO15 |
| Speaker enable | Board-specific | GPIO46 |
| Display | Varies / none | ILI9341 320x240 via SPI (CLK=7, MOSI=6, CS=5, DC=4, RST=48) |
| Touch | Varies / none | GT911 capacitive (I2C 0x5D, INT=GPIO3) |
| I2C | Board-specific | SCL=GPIO18, SDA=GPIO8 |
| Wake word | WakeNet "Hey Jarvis" | Same (ESP-IDF WakeNet) |
| Button | Board-specific | GPIO1 (mute), GPIO0 (boot) |
| PSRAM | Varies | 16MB octal mode |
| Framework | ESP-IDF v5.5+ | Same |

### Work needed
1. Create new board config in HeyClawy (like `build_box3.sh`)
2. Add ES7210/ES8311 codec init code (I2C register setup)
3. Add I2S config for BOX-3 pin mapping
4. Add display driver (ILI9341/S3BOX) for status UI
5. Add GT911 touch support for tap-to-talk
6. Add speaker enable GPIO46 control
7. Test wake word with BOX-3 microphone
8. Configure secrets (WiFi, OpenClaw host/token, TTS/STT endpoints)

## Phase 3: Flash and Test
1. Build HeyClawy with BOX-3 support
2. Flash via USB
3. Configure via built-in web UI
4. Test: say "Hey Jarvis" → hear OpenClaw respond

## Current State of ESPHome Setup (fallback)
The ESPHome/HA pipeline in `jarvis-1.yaml` is functional but fragile:
- Mic works (Whisper transcribes correctly)
- OpenClaw responds
- Speaker works (boot chime plays)
- Piper TTS is unreliable (intermittent `# channels not specified` error)
- Can be used as fallback if HeyClawy port proves too complex

## Files
- HeyClawy repo: `~/repos/HeyClawy/`
- Server config: `~/repos/83rr-poweredge/`
- ESPHome config (fallback): `~/repos/jarvis/jarvis-1.yaml`
- OpenClaw on server: `infrastructure/openclaw/docker-compose.yml`
- Server IP: 192.168.1.179
- BOX-3 IP: 192.168.1.129
- HA Yellow IP: 192.168.1.108
