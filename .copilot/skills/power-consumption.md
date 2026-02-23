# Power Consumption Analysis & Optimization Guide

## Battery Targets
- **M5StickCPlus2**: 120mAh battery → target >1 hour runtime
- **SenseCAP Watcher**: 400mAh battery → target >3 hours runtime
- **Waveshare Audio Board**: No battery (USB powered only)

## Power Budget Overview (ESP32-S3 @ 160MHz)

| Component | Active (mA) | Idle/Sleep (mA) | Notes |
|-----------|-------------|-----------------|-------|
| CPU (dual-core) | 30-50 | 30-50 | No true CPU sleep implemented |
| WiFi radio (TX) | 180-240 | 0 (modem sleep) | DTIM beacon wake ~100ms |
| WiFi radio (RX) | 95-100 | 0 (modem sleep) | |
| WiFi modem sleep | - | 5-15 | Between beacons |
| Display backlight | 20-40 | 0 | Off during light sleep |
| LCD controller | 5-10 | 5-10 | Still powered during sleep |
| Microphone (PDM) | 1-3 | 1-3 | Always on for wake word |
| ESP-SR (wake word) | 15-30 | 0 (when paused) | Neural net inference |
| Speaker codec | 5-10 | 1-2 | Quiescent |
| PSRAM | 3-5 | 3-5 | Always active |
| IO expander | 1-2 | 1-2 | SenseCAP only |
| RGB LED | 0-15 | 0 | Off during sleep |
| **Total estimate** | **~150-300** | **~50-90** | |

## Current Consumers (Ranked by Impact)

### 1. WiFi Radio — THE Dominant Consumer (~100-200mA active)

**Current state**: WiFi is the single biggest power drain. Every WebSocket message,
HTTP request, and ping forces the WiFi radio fully awake for TX/RX.

**Traffic sources (measured from logs)**:
| What | Interval (idle) | Interval (active) | WS Messages |
|------|----------------|-------------------|-------------|
| ~~WS Ping/Pong~~ | ~~1 second~~ → **30s** | ~~1s~~ → **30s** | 2 (ping+pong) |
| Health request | **Removed when idle** | 5s (fast poll) | 2 (req+res) |
| Usage.status | **Removed when idle** | 5s (fast poll) | 2 (req+res) |
| Cron.list | **60s** (was 15s in sleep) | 5s (fast poll) | 2 (req+res) |
| Server health push | ~30s (server-side) | ~30s | 1 (event) |
| Server agent events | On demand | On demand | 1 per event |

**Already implemented (this session)**:
- ✅ WS ping interval: 1s → 30s after auth (saves ~29 WiFi wakes/30s)
- ✅ Idle polling: removed health+usage requests (server pushes health)
- ✅ Sleep polling: reduced to cron.list only, every 60s (was health+tasks every 15s)

**Estimated savings**: ~60-80mA average during idle (30× fewer WiFi wakes)

### 2. Wake Word Detection — Continuous Mic + Neural Net (~15-30mA)

**Current state**: `wake_word_task` runs on Core 1 at priority 5, continuously:
1. Reads 512-sample chunks from mic via `board_audio_record()` (blocking I2S read)
2. Feeds chunks to ESP-SR WakeNet for inference
3. Never stops unless explicitly paused (during recording/TTS)

**On M5StickCPlus2**: Wake word is **disabled** (ESP-SR requires ESP32-S3).
So this only applies to SenseCAP and Waveshare Audio Board.

**Impact**: Mic ADC runs at 16kHz continuously + WakeNet neural net inference on every chunk.

### 3. CPU Always Running (~30-50mA)

**Current state**: "Light sleep" only turns off display/LED. CPU remains at 160MHz.
`CONFIG_PM_ENABLE` is NOT set — no automatic frequency scaling or CPU light sleep.

**Tasks running during "light sleep"**:
| Task | Interval | CPU Load |
|------|----------|----------|
| status_update | 500ms | Low (polls counters) |
| sleep_task | 5s | Minimal |
| knob_timer | 5ms (200Hz!) | ISR-like, very frequent |
| LVGL task | 5ms timer | Still running even with display off |
| serial_cmd | Continuous | Blocking fgetc |
| wake_word | Continuous | High (neural net) |
| WS client task | Event-driven | Waiting on socket |
| SSCMA tasks | Continuous | Camera monitoring |

### 4. Display (~25-50mA when on)

**Current state**: Backlight turns off during sleep (good). LCD controller stays powered.

### 5. SSCMA Camera Tasks (~5-10mA)

**Current state**: Two tasks (`sscma_client_process`, `sscma_client_monitor`) run
continuously on SenseCAP for AI camera monitoring, even when not in use.

---

## Optimization Suggestions (Ranked: Most to Least Low-Hanging Fruit)

### ✅ DONE — WS Ping Interval Relaxation
**Impact**: HIGH | **Effort**: Trivial | **Trade-off**: None
- Changed from 1s to 30s after auth completes
- 1s was only needed for challenge data flush during handshake
- Saves ~29 unnecessary WiFi TX/RX cycles per 30 seconds

### ✅ DONE — Idle Polling Reduction
**Impact**: MEDIUM | **Effort**: Trivial | **Trade-off**: None
- Removed `usage.status` and `health` requests during idle (server pushes health)
- Reduced sleep cron polling from 15s to 60s
- Idle polling now: tasks only, every 60s (was health+usage+tasks every 30s)

### ✅ DONE — WiFi Power Save MAX_MODEM
**Impact**: HIGH (~20-40mA savings) | **Effort**: Low | **Trade-off**: Slight latency
```c
esp_wifi_set_ps(WIFI_PS_MAX_MODEM);  // M5Stick: always MAX; SenseCap: MAX during sleep, MIN when active
```
- **M5Stick**: Always `MAX_MODEM` after WiFi connect (`wifi_manager.c` IP_EVENT handler)
- **SenseCap/Audio**: `MIN_MODEM` when active → `MAX_MODEM` when entering light sleep (`sleep_task`) → back to `MIN_MODEM` on wake (`app_reset_activity_timer`)
- Increases WiFi latency by ~100-300ms for incoming messages during sleep
- In practice: no real-time events expected during display-off sleep anyway

### 2. Pause Wake Word During Light Sleep
**Impact**: MEDIUM (~15-25mA) | **Effort**: Low | **Trade-off**: Must press button to interact
- Wake word detection continuously runs mic + neural net even during display-off sleep
- Could pause detection when in light sleep, resume on button press
- **Trade-off**: User must press knob/button instead of saying "Hey Jarvis" to wake
- **Compromise**: Make this configurable in WebUI ("wake word during sleep: on/off")

### ✅ DONE — Reduce Knob Timer to 20ms (50Hz)
**Impact**: LOW-MEDIUM (~5mA from reduced ISR overhead) | **Effort**: Trivial | **Trade-off**: None
- Currently 5ms (200Hz) — way too fast for human hand rotation
- 20ms (50Hz) is perfectly responsive for UI knobs
- Can also stop the timer entirely during light sleep (knob not needed)
- **M5Stick**: No knob, not applicable

### 4. Enable CONFIG_PM_ENABLE (CPU Frequency Scaling)
**Impact**: MEDIUM (~10-20mA) | **Effort**: Medium | **Trade-off**: Requires testing
- Enables automatic CPU frequency scaling based on load
- CPU drops to 80MHz or lower when idle tasks dominate
- Requires `CONFIG_FREERTOS_HZ=1000` (already set)
- **Risk**: May affect I2S timing, LVGL rendering, or WiFi stability
- Must test thoroughly on all 3 boards
- Cannot use true light sleep with WiFi connected (loses connection)

### 5. Stop LVGL Task During Sleep
**Impact**: LOW (~3-5mA) | **Effort**: Medium | **Trade-off**: Minor
- LVGL runs its handler every 5ms even when display is off
- Could pause LVGL timer during light sleep, resume on wake
- Need `lvgl_port_lock()` to safely pause/resume
- **Trade-off**: First frame after wake may be delayed 10-20ms

### 6. Stop SSCMA Tasks When Not In Use
**Impact**: LOW-MEDIUM (~5-10mA) | **Effort**: Medium | **Trade-off**: Camera startup delay
- AI camera tasks run continuously but camera is rarely used
- Could suspend/resume tasks on demand (when camera button pressed)
- **Trade-off**: ~500ms startup delay when accessing camera

### 7. Duty-Cycle WiFi During Deep Idle
**Impact**: VERY HIGH (~80-100mA) | **Effort**: HIGH | **Trade-off**: Loss of real-time
- Periodically disconnect WiFi, sleep CPU, reconnect every N minutes
- During sleep: no wake word, no display, no real-time events
- On wake: reconnect WiFi (2-3s), SNTP resync, WebSocket reconnect (1-2s)
- **Trade-off**: 5-10 second gap where device is unreachable
- **Use case**: Battery saver mode for overnight/extended idle
- Could be triggered by very long idle (>30 minutes)

### 8. Reduce CPU Frequency to 80MHz
**Impact**: MEDIUM (~10-15mA) | **Effort**: Low | **Trade-off**: Slower processing
```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=80
```
- Halves CPU power but also halves processing speed
- Wake word detection may be slower (but should still work)
- TTS decoding slower (minimp3)
- **Trade-off**: Longer response times, may affect real-time audio
- **Not recommended** as default, but could be used in battery saver mode

---

## M5StickCPlus2 Specific Notes
- **No wake word** (ESP32, not S3) — saves ~20mA inherently
- **No camera** — saves ~5-10mA
- **Smaller display** (135×240 vs 412×412) — less backlight power
- **No knob timer** — one less periodic consumer
- **Buzzer speaker** — very low power vs codec amplifier
- **120mAh battery** at estimated ~80-120mA idle = 1-1.5 hours
- **Key wins**: WiFi MAX_MODEM + reduced polling → maybe 60-80mA → 1.5-2 hours

## SenseCAP Watcher Specific Notes
- **400mAh battery** at estimated ~100-150mA idle = 2.5-4 hours
- **Largest display** — backlight is significant power draw
- **Wake word active** — mic + ESP-SR always running
- **Camera tasks** — SSCMA always running
- **Key wins**: WiFi optimization + pause wake word during sleep → ~80-100mA → 4-5 hours

## Power Measurement
To accurately measure, use a USB power meter (e.g., Ruideng UM34C) between
USB cable and device. Monitor:
- Boot + WiFi connect: ~200-300mA (peak)
- Idle connected: ~100-150mA (current baseline)
- Idle connected after optimizations: target ~60-90mA
- Light sleep (display off): target ~40-70mA
- TTS playback: ~150-200mA (WiFi + speaker + CPU)
- Recording: ~120-160mA (mic + WiFi)

## Implementation Priority
1. ✅ WS ping relaxation (done)
2. ✅ Idle polling reduction (done)
3. ✅ WiFi MAX_MODEM (done — M5Stick always, SenseCap dynamic with sleep)
4. ✅ Knob timer 50Hz (done)
5. Pause wake word during sleep (configurable)
6. CONFIG_PM_ENABLE (needs testing)
7. LVGL pause during sleep
8. SSCMA suspend
9. Battery saver deep idle mode (major feature)
