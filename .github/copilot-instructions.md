# HeyClawy - Copilot Instructions

## Project Overview
HeyClawy is an ESP-IDF v5.5 project for ESP32 devices that interface with [OpenClaw](https://github.com/openclaw/openclaw) personal AI assistant. Supported boards: SenseCAP Watcher (ESP32-S3), Waveshare ESP32-S3 Audio Board, M5StickCPlus2 (ESP32).

## ⚠️ Current Task
See `docs/current-task.md` for the active task tracker. **Always update this file at the end of each work session.**

## Key References
- **Hardware pinout**: `docs/hardware/sensecap-watcher-pinout.md`
- **OpenClaw repo**: `../openclaw/` (cloned locally at `C:\Users\Omer\Dropbox\ESP2026\openclaw`)
- **SenseCAP Watcher firmware source**: https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware
- **Current task tracker**: `docs/current-task.md`

## Skills (read these for domain knowledge) (Scan skills folders if I missed something)
- `.copilot/skills/sensecap-watcher-hardware.md` — Hardware details, pinouts, peripherals, driver chips
- `.copilot/skills/esp-idf-build-flash.md` — How to build, flash, and configure the project
- `.copilot/skills/serial-monitoring.md` — How to monitor serial output from the device
- `.copilot/skills/openclaw-integration.md` — OpenClaw Gateway WebSocket protocol and chat API
- `.copilot/skills/openclaw-device-auth.md` — ED25519 device identity authentication (CRITICAL)
- `.copilot/skills/openclaw-api.md` — **OpenClaw Gateway API methods reference** (health, usage, chat, etc.)
- `.copilot/skills/adding-hardware-targets.md` — How to add support for new ESP32 boards
- `.copilot/skills/io-expander-power-control.md` — PCA9535 IO expander API gotchas (CRITICAL — uint8_t level bug)
- `.copilot/skills/camera-display-verification.md` — How to use the webcam to verify display output
- `.copilot/skills/openclaw-chat-response-handling.md` — Chat response accumulation, dedup fix, emoji stripping
- `.copilot/skills/tts-playback.md` — EdgeTTS integration with minimp3 decode + PSRAM memory architecture
- `.copilot/skills/esp32-memory-architecture.md` — ⚠️ CRITICAL: Internal vs PSRAM, DMA conflicts, task stack allocation
- `.copilot/skills/audio-codec-dev.md` — ⚠️ CRITICAL: Using esp_codec_dev framework for ES8311 + ES7243E (NOT raw I2C)
- `.copilot/skills/ws-auth-ping-fix.md` — WebSocket auth delay fix (ping_interval_sec=1)
- `.copilot/skills/audio-emulation.md` — Emulating voice input via PC speakers for testing
- `.copilot/skills/webserver-settings.md` — **Web server REST API, NVS settings architecture, error log component**
- `.copilot/skills/modular-architecture.md` — **Code structure: app_main.c split into modules (app_state, voice_chat, serial_cmd, app_tasks)**
- `.copilot/skills/ui-event-decoupling.md` — **UI-to-main event communication pattern (avoids circular dependency)**
- `.copilot/skills/sleep-management.md` — **Device sleep: idle timeout, display off, wheel wake**
- `.copilot/skills/spd2010-touch-driver.md` — **⚠️ CRITICAL: Custom SPD2010 touch I2C driver bypassing broken panel_io**
- `.copilot/skills/esp32s3-deep-sleep.md` — **ESP32-S3 deep sleep with GPIO2 (PCA9535 INT) wake**
- `.copilot/skills/openclaw-cron-crud.md` — **OpenClaw cron CRUD API: list, add, update, toggle, remove cron jobs**
- `.copilot/skills/stt-language-detection.md` — **STT language auto-detection, server config, non-Latin display fallback**
- `.copilot/skills/openclaw-hooks-activity.md` — **OpenClaw hooks API, external activity tracking, chat event filtering**
- `.copilot/skills/openclaw-security-patch.md` — **OpenClaw LAN security patch: ws:// on private IPs, TUI pairing fix**
- `.copilot/skills/openclaw-model-config.md` — **Adding custom model providers, local LLM config, fallback models**
- `.copilot/skills/hebrew-font-rendering.md` — **Hebrew/RTL LVGL font: generation, BiDi config, font-reset order bug**
- `.copilot/skills/camera-sscma-integration.md` — **⚠️ CRITICAL: SSCMA camera base64 decode, IO expander pin mask fix, image pipeline**
- `.copilot/skills/m5stickcplus2.md` — **M5StickCPlus2 board: ESP32 (not S3), pinout, buzzer audio, PDM mic, power hold**
- `.copilot/skills/power-consumption.md` — **Battery power analysis: ranked optimizations, WiFi/CPU/wake word power budgets**

## Architecture
- `components/board/` — Hardware abstraction layer. Each board has its own header (`board_<name>.h`) with pin definitions. Common API in `board.h`. Includes RGB LED animation system.
- `components/openclaw/` — OpenClaw WebSocket client with ED25519 device auth, chat send/receive, short response prompting
- `components/wifi_manager/` — WiFi STA connection + SNTP time sync
- `components/ui/` — LVGL-based display UI with 11-state state machine for round 412x412 display
- `components/tts/` — EdgeTTS HTTP client with WAV streaming playback through speaker
- `components/stt/` — Speech-to-text HTTP client (faster-whisper), auto-detects language
- `components/ed25519_lib/` — orlp/ed25519 C library for device identity signing
- `components/settings/` — **NVS-backed settings** with JSON import/export, secret masking. All configurable values live here.
- `components/error_log/` — **Error ring buffer** (32 entries) with timestamp, source, severity. JSON export for web UI.
- `components/webserver/` — **HTTP server** (port 80) with embedded responsive SPA. REST API for settings, status, errors.
- `main/` — Application code, split into modules:
  - `app_main.c` — Init sequence + main event loop (~240 lines)
  - `app_state.c` — Shared globals, LED mapping, state transitions
  - `voice_chat.c` — Recording → STT → OpenClaw chat flow
  - `serial_cmd.c` — Serial console command processing
  - `app_tasks.c` — Background tasks (knob monitor, status polling, TTS playback)
- `tools/` — Python utilities (device auth testing, etc.)
- Board selection via `idf.py menuconfig` → HeyClawy Application Configuration → Select target board.

## Code Conventions
- C11, ESP-IDF style (ESP_LOG*, ESP_RETURN_ON_ERROR, etc.)
- Board HAL functions prefixed with `board_`
- All GPIO pins defined as macros in board-specific headers
- Use ESP-IDF component manager for external dependencies

## Important Notes
- The SenseCAP Watcher uses a PCA9535 IO expander for power control and button detection
- **CRITICAL**: `esp_io_expander_set_level()` takes `uint8_t level` (0 or 1), NOT a bitmask. See `io-expander-power-control.md`.
- **CRITICAL**: OpenClaw final chat event includes full message — must REPLACE streaming buffer, not append. See `openclaw-chat-response-handling.md`.
- The display is SPD2010 (412x412) which needs the `esp_lcd_spd2010` managed component
- LVGL buffers must be in internal RAM (PSRAM causes DMA bounce buffer OOM). Use 10-row single buffer.
- Audio uses ES8311 (speaker) + ES7243/ES7243E (mic) codecs via `esp_codec_dev` framework, both on the same I2S bus
- **CRITICAL: Audio codec initialization**: Must use `esp_codec_dev` framework (NOT raw I2C register writes). Speaker uses `es8311_codec_new()`, mic uses `es7243e_codec_new()`. Mic must be opened with `channel=2, channel_mask=MASK(1)` for proper stereo-to-mono extraction. See `audio-codec-dev.md` skill.
- **CRITICAL: WebSocket auth ping fix**: ESP WebSocket client needs `ping_interval_sec=1` to avoid 10s delay. Default 10s ping causes challenge data to be buffered but not read. See `ws-auth-ping-fix.md` skill.
- The knob button is on the IO expander (P0.3), not a direct GPIO
- SD card and AI camera share the same SPI2 bus — CS pin management is critical
- WiFi first connection attempt sometimes fails; retry 2 succeeds
- SNTP must sync before OpenClaw connection (timestamp validation)
- Device on COM3, DTR toggle causes reset — use `port.dtr = False` before opening
- **⚠️ CRITICAL: CH342 RTS pin**: Must set `port.rts = False` when opening serial. CH342 blocks serial output if RTS is high! This is the #1 cause of "serial stops after bootloader" issues.
- A webcam ("HD Webcam eMeet C960") is pointed at the device for remote display verification
- Device is rotated 90° counterclockwise for stability
- **Intermittent auth issue ROOT CAUSE FOUND**: ESP WebSocket client library buffers challenge data during WS upgrade. Default 10s `ping_interval_sec` means first PING triggers after 10s, flushing buffered data. Server has 10s handshake timeout → connection closed before challenge is read. Fix: set `ping_interval_sec=1` in WS config. Auth now completes in ~150ms.
- **⚠️ CRITICAL: TTS/large task stacks must use PSRAM** — see `esp32-memory-architecture.md`. Using internal RAM for 16KB+ stacks causes LCD SPI DMA allocation failures.
- **Prefer logs over camera**: For debugging, always use serial logs (filter by tag) rather than trying to parse webcam images. Camera is only for visual UI verification.
- Serial commands available: `say <msg>`, `talk`, `play`, `details`, `status`, `web`, `tasks`, `cron-add-test`, `cron-remove <id>`, `abort`, `wake`, `reboot`
- **UI event decoupling**: UI component cannot depend on main module. Uses `ui_set_event_group(void*)` setter called from app_main. Event bit values duplicated in ui.c (UI_TTS_PLAY_BIT=BIT3, UI_DETAILS_BIT=BIT5, UI_WEBSERVER_TOGGLE_BIT=BIT4) — MUST stay in sync with app_state.h.
- **Sleep management**: sleep_task polls every 5s, checks `s_last_activity_us` vs `sleep_timeout_ms`. Turns off display+LED, polls wheel button for wake. Skips sleep during active states. Uses `app_reset_activity_timer()`.
- **OpenClaw cron API**: `cron.list` with `includeDisabled: true` returns `{"jobs": [...]}`. Job UUID is 36 chars — `id[40]` in struct. `cron.update` with `patch: { enabled: true/false }` to toggle. `cron.add` requires scope `operator.admin` and payload with `kind: "agentTurn"` + `"message"` field (NOT `"text"`). Polled every 5s when tasks running, 30s otherwise.
- **Tasks screen**: `ui_tasks.c` — second LVGL screen with scrollable task list. Tap task to toggle enabled/disabled (sends `cron.update`). Wheel click/touch empty = back to main. Wheel spin = scroll.
- **Web server**: Toggle with `web` serial command. Runs on port 80. Status bar shows globe icon when active. Shows device IP for 5s on start. REST API includes `/api/tasks` and `/api/tasks/toggle` for cron job management.
- **Web UI tabs**: Dashboard (live status), Device (volume/brightness/silence/WiFi), OpenClaw (host/port/token/wizard), Tasks (cron job list + toggle), Services (TTS/STT config), Errors (log viewer)
- **OpenClaw device pairing**: Adding `operator.admin` scope requires re-approval. Approve via SSH: edit `~/.openclaw/devices/paired.json` to add scope, clear `pending.json`, restart gateway (`kill <pid>`).
- **OpenClaw API**: Use `health` method for live status (sessions, activity, WA channel), `usage.status` for cost data (may be empty if no provider auth configured). See `openclaw-api.md` skill.
- **OpenClaw health response dispatch**: In `openclaw_client.c`, type:"res" responses are checked in order: usage (has `providers` key) → health (has `ok` + `agents`) → connect (state==AUTHENTICATING). Each branch does `goto cleanup`.
- **OpenClaw code**: Available locally at `C:\Users\Omer\Dropbox\ESP2026\openclaw`. Pull latest changes before exploring it.
- **SSH to OpenClaw server**: See `secrets.txt` for SSH credentials. Data dir: `~/.openclaw`
- **Recording**: Adaptive noise floor calibration (skip 4 chunks I2S startup, measure 16 chunks, threshold = max(configured, noise_floor*2.5)). No-speech timeout at 5s. Silence detection at 1200ms. Max 15s. Start/stop feedback tones (embedded PCM).
- **STT language**: Auto-detected by faster-whisper (no language parameter sent). Works with any language.
- **STT server**: Custom `stt_server.py` at `~/.openclaw/tools/` on the OpenClaw server (`<OC_HOST>:5051`, see secrets.txt). Model: "small", beam_size=5. Auto-starts via `@reboot` crontab entry. NOT a Docker container — native Python process. Health check: `curl http://localhost:5051/health`. See `stt-language-detection.md` skill.
- **Non-Latin display**: Montserrat font lacks Hebrew/Arabic/CJK glyphs. **Hebrew is now supported** via custom `lv_font_hebrew_28` and `lv_font_hebrew_36` fonts (generated from NotoSansHebrew, with Montserrat fallback for digits/Latin). `has_hebrew()` detects Hebrew text → uses `sanitize_text_hebrew()` (strips nikud/cantillation, keeps only letters U+05D0-U+05EA) + Hebrew font + `LV_BASE_DIR_RTL`. Non-Hebrew non-Latin still falls back to "✓" + "Tap ▶ to hear". See `hebrew-font-rendering.md` skill. **⚠️ CRITICAL**: `ui_set_response()` must be called AFTER `app_set_state(UI_STATE_RESPONSE)` — `ui_set_state()` resets font to Montserrat, so Hebrew font must be set last.
- **TTS Hebrew**: `tts_speak()` auto-detects Hebrew text (UTF-8 lead bytes 0xD6-0xD7) and switches from default voice "alloy" to "he-IL-HilaNeural" for EdgeTTS compatibility. "alloy" voice returns HTTP 500 for Hebrew text.
- **⚠️ CRITICAL: SPD2010 touch driver**: The managed component's `panel_io_i2c_v2` breaks with ESP-IDF v5.5's i2c_master driver (zero-length write rejected). Must use custom direct I2C driver. See `spd2010-touch-driver.md` skill.
- **Touch events**: LVGL `LV_EVENT_CLICKED` on big label triggers KNOB_PRESSED_BIT in IDLE state. `LV_EVENT_PRESSED` on screen triggers TOUCH_BIT for tick sound + activity reset.
- **Wheel state machine**: Documented in `docs/wheel-state-machine.md`. Key: RESPONSE + click = new recording (not TTS). Sleep + click = wake only.
- **Long press deep sleep**: Changed from `board_power_off()` (caused immediate reboot via IO expander) to `esp_deep_sleep_start()` with GPIO2 wake.
- **Sleep wake**: Any serial command resets activity timer and wakes display from sleep. `app_reset_activity_timer()` restores display brightness + LED when waking. **`app_just_woke()`** provides 800ms cooldown after wake to prevent accidental recording trigger on touch/knob wake.
- **Sleep modes**: Light sleep = display/LED off, CPU running, WiFi connected (automatic on idle timeout). Deep sleep = full CPU shutdown, RTC wake via GPIO2 (PCA9535 INT pin) — **ONLY triggered by long wheel press (4s)**, never automatically. Deep sleep on ESP32-S3 = full cold reboot (~20s WiFi+SNTP+OpenClaw reconnect), so automatic deep sleep creates a reboot loop.
- **I2C NACK at ~2s**: Two harmless NACK errors appear right after WiFi starts scanning. PCA9535 I2C transactions collide with WiFi init but recover on next attempt. Do not try to fix these.
- **External activity tracking (MVP12 redesign)**: Device now handles ALL WebSocket event types for activity detection:
  - `agent` events: lifecycle (start/end), tool use (name + detail), assistant (responding)
  - `cron` events: job start/finish with name lookup from cached task list
  - `chat` events: delta/final/error + newly handled `aborted` state
  - `health` events: fully parsed (uptimeMs, sessions.count, sessions.recent age, channels)
  - Tracks: `is_active`, `is_external`, `active_run_id`, `active_session_key`, `active_detail`, `active_started_ms`, `active_count`
  - Abort external: wheel spin during IDLE + external activity → `openclaw_chat_abort_session(sessionKey)`
  - Elapsed timer: shows detail + seconds since activity started
  - RGB LED: active → blue breathe, bg tasks → teal breathe, recent → dim blue
  - Health fallback: if `last_activity_sec < 5` from health push → mark active (with 10s cooldown after explicit clear to prevent re-activation)
  - `clear_activity()` helper: clears all tracking fields + sets `active_cleared_ms` cooldown timestamp
  - Tool events: registered via `caps: ["tool-events"]` in connect request. Tool events are per-run/per-initiator (device-initiated only).
  - `format_tool_label()`: converts tool names ("memory_search" → "Memory", "exec" → "Exec")
  - UI restore: any activity text → "READY" when `is_active` becomes false (not just "ACTIVE" → "READY")
  - Response dismiss: touch or wheel spin during UI_STATE_RESPONSE → dismiss to IDLE
  - **CLI pairing**: `openclaw agent` CLI must be paired via `openclaw devices approve <requestId>` to go through gateway. Without pairing, CLI falls back to embedded mode (invisible to device).
- **OpenClaw hooks API**: External HTTP POST endpoint at `http://<OC_HOST>:18789/hooks/agent` for triggering agent turns. Requires separate token (see secrets.txt `HOOKS_TOKEN`) from gateway auth token. Must be explicitly enabled in `openclaw.json` with `hooks.enabled: true`. Used for testing and external integrations.
- **Camera button**: Added to UI button bar (📷 icon). Captures JPEG from Himax HX6538 AI camera via SSCMA SPI2. Image is decoded from base64 and stored in PSRAM, then sent as attachment with next voice/text chat to OpenClaw. See `camera-sscma-integration.md` skill for critical base64 decode and IO expander pin mask fixes.
- **Cancel recording**: Touch anywhere during recording or wheel spin during recording now cancels via CANCEL_BIT.
- **OpenClaw LAN security patch**: `isSecureWebSocketUrl()` in installed JS bundle (`net-COi3RSq7.js`) patched to accept RFC1918 private IPs. This allows `openclaw tui` and WebUI to work over LAN without SSL. Re-run `tools/patch_oc_security.py` after any OpenClaw npm update. See `openclaw-security-patch.md` skill.
- **Local model (SmolLM3 3B)**: Configured as fallback model at `lmstudio/smollm3-3b` via LM Studio API (see secrets.txt for `LM_STUDIO_HOST`). Zero cost. Configured in `openclaw.json` under `models.providers.lmstudio`. See `openclaw-model-config.md` skill.

## End of Session Checklist
1. Update `docs/current-task.md` with current progress
2. Update relevant skills with new learnings
3. Update this file if architecture or key notes changed
4. Create new skills for any non-trivial procedures learned
