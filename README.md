# ESP32-S3-BOX-3 — HeyClawy Setup

Voice assistant using ESP32-S3-BOX-3 → OpenClaw (via NGINX) with EdgeTTS + faster-whisper.

> Fork of [omeriko9/HeyClawy](https://github.com/omeriko9/HeyClawy) with ESP32-S3-BOX-3 board support added.

## Architecture

```
ESP32-S3-BOX-3 (mic/speaker/display)
    ├── WebSocket → NGINX (ws://openclaw.ops.markcheli.com:80) → OpenClaw
    ├── HTTP POST → faster-whisper (http://192.168.1.179:5051) — speech-to-text
    └── HTTP GET  ← EdgeTTS (http://192.168.1.179:5050) — text-to-speech
```

## Server Services (PowerEdge at 192.168.1.179)

| Service | Port | Image | Purpose |
|---------|------|-------|---------|
| EdgeTTS | 5050 | `travisvn/openai-edge-tts` | Text-to-speech (OpenAI-compatible) |
| faster-whisper | 5051 | `fedirz/faster-whisper-server` | Speech-to-text (Whisper small, CPU) |
| OpenClaw | via NGINX:80 | `ghcr.io/openclaw/openclaw` | AI conversation agent |

Server config lives in [83rr-poweredge](https://github.com/MCheli/83rr-poweredge) (`docker-compose.yml`).

## Build & Flash

Requires ESP-IDF v5.5+ (`~/esp/esp-idf`).

```bash
git clone https://github.com/MCheli/HeyClawy.git
cd HeyClawy

# Create secrets (copy template and fill in values)
cp main/include/secrets.h.example main/include/secrets.h
# Edit secrets.h — see secrets section below

# Build and flash
source ~/esp/esp-idf/export.sh
./build_box3.sh
idf.py -p /dev/cu.usbmodem1101 flash
```

## secrets.h Values

| Define | Value | Notes |
|--------|-------|-------|
| `SECRETS_WIFI_SSID` | Your WiFi SSID | 2.4GHz recommended |
| `SECRETS_WIFI_PASSWORD` | Your WiFi password | |
| `SECRETS_OPENCLAW_HOST` | `openclaw.ops.markcheli.com` | Via NGINX plain HTTP proxy |
| `SECRETS_OPENCLAW_PORT` | `80` | NGINX port (not 18789 direct) |
| `SECRETS_OPENCLAW_TOKEN` | Gateway auth token | From OpenClaw config |
| `SECRETS_DEVICE_KEY_HEX` | 64-char hex ED25519 seed | Generate: `python3 -c "import os; print(os.urandom(32).hex())"` |
| `SECRETS_TTS_HOST` | `192.168.1.179` | PowerEdge IP |
| `SECRETS_TTS_PORT` | `5050` | EdgeTTS port |
| `SECRETS_TTS_API_KEY` | `""` | Not needed for EdgeTTS |
| `SECRETS_TTS_VOICE` | `en-US-AndrewNeural` | Or any Edge TTS voice |
| `SECRETS_STT_HOST` | `192.168.1.179` | PowerEdge IP |
| `SECRETS_STT_PORT` | `5051` | faster-whisper port |

## Device Pairing

After first flash, the device will connect to OpenClaw but get "pairing required". Approve on the server:

```bash
docker exec openclaw openclaw device approve <device-identity-hex>
```

The device identity is logged at boot and displayed in the serial monitor.

## Hardware Notes

- **Display**: ILI9341 320x240, reset pin GPIO48 is active HIGH (inverted)
- **Touch**: GT911 at I2C address 0x14 (not the default 0x5D)
- **Battery dock**: 18650 module reads via ADC on GPIO10, toggle switch must be ON
- **Sleep**: Stays awake when battery >= 95% or charging; wake word active during sleep
- **Wake word**: "Hey Jarvis" (WakeNet9 model)

## ESPHome Fallback

If HeyClawy has issues, `devices/esp32-s3-box-3/esphome-fallback.yaml` contains the working
ESPHome/Home Assistant voice pipeline config. Flash via ESPHome dashboard.

## Key Files

| File | Purpose |
|------|---------|
| `build_box3.sh` | Build script for BOX-3 |
| `components/board/include/board_esp32s3box3.h` | Pin definitions and capability flags |
| `components/board/board.c` | Hardware init (display, touch, audio, battery) |
| `components/ui/ui.c` | 320x240 UI layout |
| `devices/esp32-s3-box-3/PLAN.md` | Original implementation plan |
| `devices/esp32-s3-box-3/esphome-fallback.yaml` | ESPHome fallback config |
| `main/include/secrets.h.example` | Secrets template |
