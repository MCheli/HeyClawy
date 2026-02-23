# Camera — SSCMA SPI Integration (Himax HX6538)

## Overview
The SenseCAP Watcher has an AI camera chip (Himax HX6538) connected via the SSCMA protocol over SPI2. This skill documents the camera initialization, capture, and image pipeline.

## Hardware Setup
- **Camera Chip**: Himax HX6538 (AI Camera, ID=360779f5, FW=2024.08.16)
- **Bus**: SPI2 (shared with SD card)
  - SCLK=GPIO4, MOSI=GPIO5, MISO=GPIO6
  - Camera CS=GPIO21 (direct GPIO)
  - SD Card CS=GPIO46 (must be HIGH before SPI2 access)
- **Control**: IO Expander PCA9535 at 0x21
  - Reset pin: P0.7 (output) — needs `(1ULL << 7)` bitmask
  - Sync pin: P0.6 (input) — needs `(1ULL << 6)` bitmask
- **SPI Clock**: 12 MHz, max transfer: 32KB

## CRITICAL: IO Expander Pin Mask Bug
The `esp_io_expander` API expects **bitmasks** (e.g., `1ULL << 7` = 128), NOT pin numbers.
The original SSCMA client code passes raw pin numbers (e.g., `7`), causing wrong pins to be manipulated.

**Fix applied in**:
- `components/sscma_client/src/sscma_client_ops.c` — 4 calls fixed
- `components/sscma_client/src/sscma_client_io_spi.c` — 3 calls fixed

Pattern: `esp_io_expander_set_dir(io_exp, (1ULL << pin_number), ...)` instead of `esp_io_expander_set_dir(io_exp, pin_number, ...)`

## Image Data Format
**SSCMA returns images as base64 strings**, not raw bytes!

`sscma_utils_fetch_image_from_reply()` in `sscma_client_ops.c`:
- Extracts `data.image` from JSON reply
- Returns a `char *` string (base64-encoded JPEG)
- `image_size` = `strlen(image_str)` = length of base64 string

**You MUST decode the base64** before treating as raw JPEG. Otherwise you get double-encoding when sending to OpenClaw.

```c
// In camera.c callback — decode base64 to raw JPEG
size_t decoded_len = 0;
mbedtls_base64_decode(NULL, 0, &decoded_len, (const unsigned char *)image_data, image_size);
s_captured_jpeg = heap_caps_malloc(decoded_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
mbedtls_base64_decode(s_captured_jpeg, decoded_len, &actual_len,
                      (const unsigned char *)image_data, image_size);
```

## Capture Flow
1. `camera_init()` → SPI2 bus → SSCMA IO → SSCMA client → register callbacks → `sscma_client_init()`
2. `camera_capture_jpeg()` → `sscma_client_sample(client, 1)` → waits on semaphore (5s timeout)
3. Callback `sscma_on_event()` → `sscma_utils_fetch_image_from_reply()` → base64 decode → store in PSRAM
4. Returns pointer to raw JPEG + size

## Sending to OpenClaw
Use `openclaw_chat_send_with_image(message, jpeg_buf, jpeg_size, callback)`:
- Encodes raw JPEG to base64
- Sends as attachment: `{"type":"image","mimeType":"image/jpeg","fileName":"camera.jpg","content":"data:image/jpeg;base64,..."}`
- OpenClaw strips the data URL prefix, sniffs MIME, passes to vision model

## Typical Image Sizes
- Base64 from SSCMA: ~2400-2500 chars
- Decoded JPEG: ~1800-1900 bytes (very low resolution from AI camera)
- Valid JPEG header: FF D8 FF

## Known Issues
- 2 benign I2C NACK errors during init (SSCMA internal timing)
- Image resolution is very low (AI camera optimized for inference, not photography)
- TTS fails for Hebrew responses (HTTP 500 from TTS server)

## Key Files
- `components/camera/camera.c` — Init, capture, callbacks
- `components/sscma_client/` — SSCMA protocol client (SPI IO, ops)
- `components/openclaw/openclaw_client.c` — `openclaw_chat_send_with_image()`
- `main/app_main.c` — CAMERA_BIT handler, g_pending_jpeg globals
- `main/voice_chat.c` — Voice path image attachment
- `main/serial_cmd.c` — `cam`/`camera` + `say` commands with image support
