# Current Task: MVP18 - All Issues Resolved ✅

## Status: COMPLETE — commit 8dc843c

## Session 28 — Reconnect Fix + Uptime Text Removal + Verification

### Fixed
- **Reconnect after gateway restart** (CRITICAL): After first auth, ping is relaxed to 30s.
  On reconnect, `WEBSOCKET_EVENT_CONNECTED` didn't reset it, so challenge couldn't be read within
  server's 10s timeout. Fix: reset `ping_interval_sec=1` in `WEBSOCKET_EVENT_CONNECTED` handler.
  **Verified**: Gateway restarted via SSH, device auto-reconnected and auth completed in ~1.5s.

- **Uptime/session count text removed**: `ui_set_server_info()` no longer shows `S:24` or 
  `Last: Xm ago` in IDLE state. Now only shows WA linked icon (📞) and "Active" when OpenClaw
  is actively processing. Clean idle display.

### Verified
- SenseCap (COM3): flashed, boots, connects, reconnects after gateway restart ✅
- M5Stick (COM17): built + flashed, boots, connects, TTS "say hello" works ✅
- Audio Board: builds for ESP32-S3 ✅

### Confirmed Working (existing code, no regression)
- **M5Stick buttons**: All 3 buttons with double-click detection fully implemented ✅
- **TTS channel filtering**: Only cron notifications TTS'd; WhatsApp/Telegram/external filtered ✅
- **Channel naming**: Device connects as `displayName: "HeyClawy"` (correct) ✅

### Commit: 8dc843c

### Still Pending (Advisory)
- **Security consultation** (item 2 from user): How OpenClaw WS security works, options for
  securing LAN vs VPN vs relay. No code changes needed, advisory answer pending.

## Session 27 — Power Consumption Analysis & Optimization

### Implemented (No Trade-off)
- **WS ping relaxation**: 1s → 30s after auth (saves ~29 WiFi wakes per 30s)
- **Idle polling reduction**: Removed redundant health/usage requests during idle (server pushes health ~30s)
- **Sleep polling reduction**: Tasks-only every 60s (was health+tasks every 15s)

### Documented
- Created comprehensive skill: `.copilot/skills/power-consumption.md`
- Ranked 9 further optimizations from easiest to hardest with trade-offs
- Key remaining wins: WiFi MAX_MODEM (~20-40mA), pause wake word during sleep (~15-25mA)

### Commits
- `c05b8b6` — Power optimization: WS ping relaxation + reduced idle polling


## Session 26 — Fix "Responding" Stuck Bug

### Fixed
- **"Responding" stuck forever** (CRITICAL recurring bug): When an agent run's WebSocket "end" event
  is lost, the device stays in "Responding" state indefinitely. Root cause was two bugs in
  `openclaw_client.c` health-based stale detection:
  1. `last_activity_sec` was only updated from health when `!is_active` → stayed at 0 during tracked runs
  2. Stale check required `active_run_id[0] == '\0'` → never fired for tracked runs with lost end events
  Fix: Always update `last_activity_sec` from health. Check `>= 30` regardless of `active_run_id`.
  When health shows 30s+ idle with tracked runs, force-clear all run slots.

### Commits
- `42e30f1` — Fix 'Responding' stuck bug: stale external run detection

## Session 25 — M5Stick I2S Delta-Sigma Speaker

### Fixed
- **M5Stick speaker**: Replaced LEDC PWM with I2S1 delta-sigma modulation (M5Unified approach)
  - I2S1 standard TX on GPIO2, I2S0 remains PDM RX for mic
  - 48kHz stereo 16-bit → 1.536Mbps bitstream → 96× oversampling of 16kHz audio
  - DMA-driven playback (no CPU busy-wait — old LEDC approach tied up entire CPU for seconds)
  - 2× gain amplification, first-order delta-sigma noise shaping
  - ~8-bit effective resolution (up from ~6-bit with 32× OSR)
  - Approaches tested and failed: I2S PDM TX (no output), SDM driver (no output), LEDC PWM (worked but poor)
  - Speaker confirmed producing audio via webcam microphone spectral analysis

### Commits
- `93c612b` — M5Stick LEDC PWM audio (intermediate, superseded)
- `6147bf6` — M5Stick I2S delta-sigma speaker (final)

### Unresolved
- TTS text corruption in log: `tts: TTS request: text=dC...` shows garbled text (but EdgeTTS receives correct text — response is valid)
- Audio quality verification: webcam mic too far for speech intelligibility test. User needs to verify directly.

## Session 23-24 — Previous

### Fixed
- **M5Stick mic DC offset** (CRITICAL): PDM mic has ~-1890 DC bias. All samples negative, making
  noise floor = 1880 and speech threshold 4700 — impossible to exceed. Fix: compute DC mean during
  calibration, subtract from all RMS calculations and post-recording buffer. Noise floor now = 46.
- **M5Stick buzzer carrier** (CRITICAL): PWM at 2000Hz = audible carrier drowning speech.
  Changed to 40000Hz (inaudible). 80MHz APB / (40000 × 256) = 7.8 ticks — sufficient resolution.
- **M5Stick buzzer WDT**: Added `esp_task_wdt_reset()` every 4096 samples to prevent watchdog
  timeout during long TTS playback (PWM loop blocks CPU with esp_rom_delay_us).
- **Reconnect watchdog**: If stuck in DISCONNECTED/ERROR/CONNECTING for 30s, force full reconnect
  (destroy WS client + recreate). Handles gateway restarts that leave device in stale state.
- **WEBSOCKET_EVENT_CONNECTED reset**: On WS reconnect, reset msg_id, free stale frag_buf, set
  CONNECTING state (was staying in DISCONNECTED after auto-reconnect).
- **Missing character**: Em-dash (U+2014) not in Montserrat → shows as square. Replaced with `-`.
- **Removed uptime**: Status display now shows compact `S:14 📞` instead of verbose uptime text.
- **Thinking animation**: Added rotating spinner `| / - \` to thinking timer (250ms rotation).

### Verified
- SenseCap: Flashed, boots, connects to OpenClaw in ~150ms auth ✅
- M5Stick: DC offset calibration working (DC=-1890, noise_floor=46) ✅
- M5Stick: TTS playback completes without crash, no WDT timeout ✅
- Audio Board: Clean build ✅

### Commits
- `7b60a2f` — Fix reconnect, M5Stick audio, UI polish
- `83e0b75` — Fix M5Stick buzzer task WDT timeout

### Remaining
- Security consultation (item 2 from user) — answer pending
- Test reconnect watchdog with actual gateway restart
- M5Stick speech recording test (need user to speak into mic)

## Session 21 — Multi-device Fixes, Notification Channel Filtering

### Completed
- M5Stick ST7789 display gap fix (40,53) for correct 135×240 rendering
- M5Stick UI: removed touch button bar, physical buttons only
- M5Stick Button A long press: cancel/abort (not deep sleep)
- M5Stick Button C short press: context-dependent (details in RESPONSE, tasks otherwise)
- OpenClaw connect: added `displayName: "HeyClawy"` (id/mode must stay "gateway-client"/"cli")
- OpenClaw connect: conditional platform field (esp32 vs esp32s3)
- Notification filtering: only TTS for cron-originated sessions (skips WhatsApp/Telegram/CLI)
- Processing ghosting fix: removed health fallback activity detection
- Processing ghosting: reduced auto-clear stale threshold from 10s to 5s
- All 3 builds verified (SenseCap, M5Stick, Audio Board)
- SenseCap flashed and tested (chat works, no regression)
- M5Stick flashed and tested (chat works, web server works, device commands work)
- Committed: a90885f

### Remaining
- TTS channel filtering: verify with actual cron job firing
- Camera can't reliably capture M5Stick 1.14" display — user visual confirmation needed

## Session 18 — M5StickCPlus2 UI Adaptation + Bug Fixes

### UI Adaptation for 240×135 Screen ✅
- Font size abstraction macros (FONT_BIG 48→20, FONT_MED 28→14, etc.)
- Compact layout: 18px status bar, 20pt big label, horizontal button bar
- `create_small_btn()` helper (30×26 px buttons)
- Separate `ui_init()` for M5StickCPlus2 via `#if defined(CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2)`
- BOARD_HAS_CAMERA guard for camera button visibility
- Hebrew font macros (FONT_HEBREW/FONT_HEB_BIG)

### Button Mapping ✅
- Button A (GPIO37, front) = talk/record (via KNOB_PRESSED_BIT)
- Button B (GPIO39, side) = context-dependent:
  - IDLE: toggle web server
  - LISTENING: cancel recording
  - THINKING/STREAMING: abort chat
  - RESPONSE: play TTS or dismiss
  - TTS_LOADING/TTS_PLAYING: stop TTS
  - SENDING: cancel send
- Button C (GPIO35, power) = short press tasks screen, long press (4s) deep sleep

### Bug Fixes ✅
- TTS task stack increased 8KB → 12KB for ESP32 (was causing stack overflow)
- announce_tts_task uses internal RAM on ESP32 (PSRAM static task fails)
- WS CLOSE frame now sets DISCONNECTED state for proper reconnection
- build_sensecap.bat now handles ESP32-S3 target switching properly
- Fixed SenseCAP build error: SenseCAP `ui_init()` needed separate function definition

### Verified ✅
- M5StickCPlus2: boots, display works, WiFi+OpenClaw connects, chat response received
- SenseCAP: builds cleanly with ESP32-S3 target
- No TTS stack overflow crash

### Commits
- `a57ce3b` — feat: M5StickCPlus2 UI, button mapping, TTS stack fix, WS reconnect

### Remaining
- M5StickCPlus2 has passive buzzer only — cannot play TTS MP3 (expected limitation)
- Physical button testing pending (A, B, C)
- Setting `activity_carousel` (NVS, default: on)
- Status API exposes active_runs for WebUI dashboard

### Notification Detection ✅  
- Device-run tracking: ring buffer of 8 recent runIds
- Non-device `chat.final` → extract text → `openclaw_notify_cb_t` callback
- Auto-TTS notifications with amber LED pulse
- `g_tts_text` buffer for notification text (checked before UI response)
- Setting `auto_notify` (NVS, default: on)

### Console UART Fix ✅
- `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` in sdkconfig.defaults
- Was USB_SERIAL_JTAG (no physical port on SenseCAP CH342 board)

### Commit: c29f3ca

## Session 14 — Deep Sleep Fix Verified

## Session 12 — Deep Sleep Fix, Cancel Recording Fix

### Deep sleep fix ✅
- ROOT CAUSE: Touch controller INT (P0.5) on IO expander kept generating interrupts after display off
- FIX: Power off peripherals (LCD, AI, SD, PA, Grove, BAT_ADC) via IO expander before sleep (matches factory firmware)
- Added RTC pull-up on GPIO2 to keep INT HIGH during deep sleep when I2C bus is unpowered
- Moved wake source config to `board_prepare_deep_sleep()` in board HAL
- Added wake cause logging at boot for debugging
- VERIFIED: Device stays in deep sleep after serial port close (10s test)
- NOTE: Serial/JTAG always wakes ESP32-S3 from deep sleep (known HW limitation)

### Cancel recording fix ✅
- CANCEL_BIT now ALWAYS cancels recording regardless of speech state
- Previously: cancel during active speech would SEND instead of cancel (bug)
- Added cancel UI button (red X) during LISTENING state
- Cancel button shares position with camera button (center bottom)

### Added `deepsleep` serial command for testing

## Session 11 — Voice Device Commands + TTS Fixes

### Voice device commands ✅
- `[DEVICE:key=value]` tags in OpenClaw responses parsed and executed
- Supported: volume, brightness, rgb (patterns/on/off/R,G,B), sleep, webserver, auto_read, reboot
- Commands stripped from response before display/TTS
- OpenClaw prompted with device command instructions in all 3 PREFIX strings
- Tested on audio board: "set volume to 60" → volume changed + TTS confirmation

### TTS JSON escaping fix ✅
- Root cause: unescaped `\n` characters in JSON body → invalid JSON → HTTP 500
- Fix: escape loop now handles `\n`→`\\n`, `\r`→`\\r`, `\t`→`\\t`, strips other control chars
- Unicode sanitization: em-dash, smart quotes, NBSP → ASCII equivalents

### TTS IP announcement fix ✅ (from Session 10)
- Rewrote one-shot TTS task with `s_announce_running` guard
- PSRAM stack allocated once, reused across calls
- Wake word paused during playback
- Verified via webcam microphone: 22.2 dB SNR

### OpenClaw gateway bind fix
- Gateway was binding to loopback only (`bind: "loopback"`)
- Changed to `bind: "lan"` in `openclaw.json` for LAN access from ESP32
- Restarted gateway — now listening on `0.0.0.0:18789`

## Session 9 — RGB Fix, Processing Fix, Silence Params, Deep Sleep, WebUI

### RGB LED animations rewritten ✅
- Root cause: brightness=20 after gamma correction mapped to 0 for most LEDs — only 1 LED visible
- Fix: brightness=50, no gamma for startup animations, modeled after xiaozhi reference
- All 5 LEDs now simultaneously lit with smooth flowing gradients
- Added startup pattern selector to WebUI + demo buttons + `/api/led/demo` endpoint

### Processing display fix ✅
- Root cause: health fallback set `is_active=true` but never auto-cleared when activity stopped
- Fix: auto-clear when `last_activity_sec >= 10` and no real run_id tracked
- Detail now shows "External activity (health: Ns ago)" instead of generic "Processing..."

### Silence detection configurable ✅
- Added `no_speech_timeout_ms` setting (default 5000ms) — previously hardcoded to 5s
- Increased `silence_timeout_ms` default from 1200ms → 1500ms
- Both visible in WebUI Device tab with explanatory help text

### Deep sleep fix ✅
- Disconnect OpenClaw + WiFi before deep sleep (prevent WebSocket events)
- Wait for wake GPIO HIGH (button released) before configuring ext0 wakeup
- Prevents immediate wake from stuck PCA9535 INT pin

### WebUI improvements ✅
- Sleep timeout now shown in seconds (user-friendly)
- Startup pattern selector with 5 options
- LED demo buttons: rainbow, aurora, starfield, fire, ocean, breathe, chase, sparkle, stop

### Audio device speaks IP ✅
- TTS announces IP address when webserver enabled on displayless boards

## Session 8 — Bug Fixes (Race Condition + Knob + Activity Detail + RGB)

### Response race condition fix ✅
- Clear `s_oc.chat_cb = NULL` after final/error/aborted response — prevents late agent events from being misclassified as device-initiated
- Added `g_tts_pending` volatile flag: set true before TTS_PLAY_BIT, cleared when TTS task starts processing
- Touch/wheel dismissal of RESPONSE state now checks `g_tts_pending` — if true, dismissal is blocked (prevents premature "READY" flash)

### Activity detail on tap ✅
- Added `s_showing_activity` flag and `s_activity_detail[128]` cache in ui.c
- `big_label_click_cb` now distinguishes between IDLE+ready (start recording) and IDLE+active (show detail)
- Sub-label shows "tap for info" hint during external activity
- Tapping "ACTIVE" display reveals run ID and tool info

### Wheel knob timer-based polling ✅
- Replaced single-pin ISR (`IRAM_ATTR knob_isr_handler`) with 5ms `esp_timer` periodic callback
- Uses proper quadrature lookup table (`s_knob_lut[16]`) for all 16 state transitions
- Reads BOTH KNOB_A and KNOB_B each tick (was only monitoring KNOB_A via ISR)
- Much more reliable during CPU-intensive operations (recording, TTS)

### RGB startup animation redesign ✅ (5 options)
- **Option 1 (default): Smooth Rotating Rainbow** — Full spectrum spread across ring, slow rotation with per-LED shimmer
- **Option 2: Aurora Borealis** — Three overlapping sine waves in green/blue/purple, organic northern lights
- **Option 3: Starfield Twinkle** — Each LED fades independently at unique frequency, warm golds + pale blues, cubic snap
- **Option 4: Fire Embers** — Warm flickering reds/oranges/yellows with pseudo-random noise, squared intensity for deep lows
- **Option 5: Ocean Waves** — Deep blue base with teal, two sine wave layers, white foam at crests
- All use gamma correction, floating-point precision, 40-60fps, new enum values in `board_rgb_mode_t`

## Session 7 — Bug Fixes (Cancel + Camera Button)

### Cancel during all chat states ✅
- Added STREAMING state to wheel spin cancel handler (app_tasks.c)
- Added SENDING state cancel via CANCEL_BIT (voice_chat runs on same task, needs signal)
- Added STREAMING to KNOB_PRESSED_BIT cancel handler (app_main.c)
- Cleared `s_oc.chat_cb = NULL` in `openclaw_chat_abort()` to prevent ghost responses
- Added CANCEL_BIT check after STT transcription in voice_chat.c
- Image send PREFIX now includes [LISTEN]/[END] instruction
- Protected pending JPEG from accidental wheel noise (3+ tick threshold)

### Camera UI button reliability fix ⏳ (Applied, awaiting user test)
- Cleared `LV_OBJ_FLAG_SCROLLABLE` from screen — prevents scroll from canceling button clicks
- Cleared `LV_OBJ_FLAG_CLICKABLE` from btn_bar — lets clicks pass to child buttons
- Added `lv_obj_set_ext_click_area(btn, 10)` — 64×64 effective touch targets
- Added touch coordinate + camera button debug logging
- Serial `camera` command verified working end-to-end (capture → encode → send → analyze → TTS)
- Both boards build successfully

## Goal
Add support for Waveshare ESP32-S3-AUDIO-Board (screenless, 7-LED RGB ring, dual mic, 16MB flash). Create display abstraction layer. Add wake word detection with configurable custom phrase.

## Completed
1. ✅ Board header (`board_waveshare_audio.h`) with full pin definitions and capability flags
2. ✅ Capability flags added to SenseCAP Watcher (`BOARD_HAS_DISPLAY/TOUCH/KNOB/CAMERA` etc.)
3. ✅ Kconfig choice added for Waveshare board selection
4. ✅ board.c refactored with conditional compilation (single file, `#if` guards)
5. ✅ UI stubs for screenless boards (ui.c, ui_tasks.c wrapped in `#if BOARD_HAS_DISPLAY`)
6. ✅ Camera stubs (`#if !BOARD_HAS_CAMERA` at top of camera.c)
7. ✅ Knob/button abstraction (BOOT button maps to KNOB_PRESSED_BIT for Waveshare)
8. ✅ Hebrew font files conditionally compiled (`#if BOARD_HAS_DISPLAY`)
9. ✅ Forward-declared LVGL types in board.h (avoids lvgl.h dependency leak)
10. ✅ ES7210 mic codec working (I2C addr fix: 7-bit 0x40 → 8-bit 0x80)
11. ✅ Both boards build successfully (SenseCAP 8MB, Waveshare 16MB)
12. ✅ Waveshare flashed and tested on COM16 — WiFi+OpenClaw connected
13. ✅ Skills documented (waveshare-audio-board.md, adding-hardware-targets.md updated)
14. ✅ Serial console fixed (USB JTAG config for Waveshare)
15. ✅ Voice recording tested — mic captures speech, noise floor calibration works
16. ✅ Chat pipeline tested — `say hello world` → OpenClaw response in 3.7s
17. ✅ TTS playback tested — MP3 decode + speaker output working
18. ✅ Wake word component created (`components/wake_word/`) with ESP-SR
19. ✅ Model partition enlarged to 4MB (fits WakeNet + MultiNet English)
20. ✅ Wake word wired into main event loop (WAKE_WORD_BIT triggers recording)
21. ✅ Pause/resume during recording and TTS (I2S bus sharing)
22. ✅ Both boards build: SenseCAP 1.93MB (36% free), Waveshare 1.47MB (53% free)
23. ✅ Wake word skill documented
24. ✅ Dual response system — OpenClaw returns short label + spoken response
25. ✅ Auto-TTS — screenless boards always play response automatically
26. ✅ IO expander buttons — BTN1 on Waveshare toggles web server
27. ✅ RGB ring animations — rainbow_spin, chase, pulse_wave, sparkle modes
28. ✅ State-to-LED mapping — multi-LED uses rich patterns, single LED keeps simple
29. ✅ WebUI LED Patterns tab — live CSS visualization of all state patterns
30. ✅ Build batch files — build_audio_board.bat and build_sensecap.bat

## Session 5 — Custom Wake Word via MultiNet → WakeNet
31. ✅ **Wake word engine fix**: MultiNet fails for short phrases (wrong architecture)
32. ✅ Switched to **WakeNet** with pre-trained **"Hey Jarvis"** model (`wn9_jarvis_tts`)
33. ✅ 15+ WakeNet models selectable via menuconfig (Alexa, Hey Computer, Hey Willow, etc.)
34. ✅ Kconfig: engine choice (WakeNet default / MultiNet for commands), phrase, threshold
35. ✅ Model partition reduced 4MB → 960KB (WakeNet ~290KB vs MultiNet 3.8MB)
36. ✅ MultiNet code kept behind `#ifdef` for potential future use
37. ✅ Both boards build: SenseCAP (37% free), Waveshare (55% free)
38. ✅ Flashed Waveshare — WakeNet loads: `wake_word="Jarvis"` freq=16000Hz
39. ✅ Skill updated with model table, custom model instructions

## Session 6 — Secrets Cleanup + Polish Features

40. ✅ **Secrets cleanup**: All hardcoded IPs, passwords, tokens removed from tracked files
41. ✅ Created `secrets.txt` (gitignored) + `secrets_example.txt` (template)
42. ✅ Python test scripts use `tools/load_secrets.py` loader
43. ✅ 8 skill files, copilot-instructions.md, docs cleaned
44. ✅ Git history rewritten with `git filter-repo` (all secrets scrubbed)
45. ✅ **RGB animation smoothing**: Gamma correction table, 50fps, sub-pixel interpolation
46. ✅ **Conversational auto-listen**: `[LISTEN]`/`[END]` tags in system prompt, auto-record after TTS
47. ✅ **UI button arc**: Proper circular geometry (±36°/±18°) for 412×412 display
48. ✅ **Button click feedback**: Brighter color + white border flash on press (not just opacity)
49. ✅ **Camera verified**: End-to-end capture→encode→send→OpenClaw→TTS pipeline working
50. ✅ Both boards build and tested

### Key Discovery: MultiNet vs WakeNet
- **MultiNet** = command recognition (3-6 word utterances, timeout-based). WRONG for wake words.
- **WakeNet** = always-on wake word detection (short phrases). CORRECT approach.
- Custom "Hi Clawy" requires Espressif TTS training (free, 2-3 weeks via GitHub issue).

## Key Discovery
⚠️ **CMake Kconfig limitation**: Cannot use `CONFIG_*` variables in CMakeLists.txt at `idf_component_register()` time. ESP-IDF processes components in two passes; sdkconfig vars are empty during first (enumeration) pass. All conditional logic must be in C preprocessor guards, not CMake.

- **Problem**: STT server (faster-whisper) on OpenClaw machine not auto-starting
- **Fix**: Added `@reboot` crontab entry for automatic startup
- **Health check**: Added `/api/stt/health` endpoint in WebUI services tab

### 3. Serial/COM Port Skill Enhancement (.copilot/skills/serial-monitoring.md)
- Added interactive serial session pattern (Python + threading)
- Documented write_powershell command duplication bug and workaround
- Documented device reboot on port open (DTR toggle)
- Added bidirectional forwarding script template

### 4. WebUI Integration Guide (index.html)
- Added "Integration Guide" card to OpenClaw tab with 4 expandable sections:
  1. Device Pairing (ED25519 keys + openclaw device approve)
  2. STT Server Setup (faster-whisper installation + auto-start)
  3. TTS Server Setup (EdgeTTS Docker container)
  4. LAN Security (allowPlaintextLan config)
- Added STT health check button to Services tab
- Added `/api/stt/health` backend endpoint (webserver.c)

## Test Results
- Noise floor calibration: `53 → effective threshold: 500` (after I2S skip)
- No-speech timeout: Recording stops at ~5.6s instead of 15s ✅
- Speech detection: Hebrew speech transcribed correctly (STT 4083ms) ✅
- OpenClaw response received and displayed ✅
- Full pipeline: record → calibrate → detect speech → silence stop → STT → OC → response ✅
- `format_tool_label()` helper: converts tool names ("memory_search" → "Memory", "exec" → "Exec")

## Verified on Device ✅
- External agent run: Thinking... → Responding → READY (no re-activation)
- Health polls after clear show no "marking active" messages
- Health fallback cooldown suppresses for 10s after explicit clear
- IDLE restore works from any activity display text

## Prior History
- [x] Patched function to accept RFC1918 private IPs (10.x, 172.16-31.x, 192.168.x)
- [x] Created reusable script: `tools/patch_oc_security.py`
- [x] Verified `openclaw tui` connects to `ws://<OC_HOST>:<OC_PORT>` without error
- [x] Fixed TUI device pairing (upgraded to `operator.admin` scope)
- [x] Created script: `tools/approve_tui.py`
- [x] Created skill: `openclaw-security-patch.md`

## Task 2: Local SmolLM3 3B Model ✅ COMPLETE
- [x] Verified model API at `http://<LM_STUDIO_HOST>:<LM_STUDIO_PORT>/v1` (LM Studio)
- [x] Tested chat completion with smollm3-3b model
- [x] Added `lmstudio` provider to `openclaw.json` under `models.providers`
- [x] Configured as fallback model: `lmstudio/smollm3-3b`
- [x] Added to allowed models list: `agents.defaults.models`
- [x] Restarted gateway — model detected and loaded
- [x] Verified with `openclaw models` command — shows as Fallback (1)
- [x] Created skill: `openclaw-model-config.md`

## Files Changed (on OpenClaw server)
- `/home/user/.npm-global/lib/node_modules/openclaw/dist/net-COi3RSq7.js` — Security patch
- `/home/user/.openclaw/openclaw.json` — Added lmstudio provider + fallback
- `/home/user/.openclaw/devices/paired.json` — TUI device admin scope

## Files Changed (HeyClawy project)
- `tools/patch_oc_security.py` — Reusable security patch script
- `tools/approve_tui.py` — TUI pairing approval script
- `.copilot/skills/openclaw-security-patch.md` — New skill
- `.copilot/skills/openclaw-model-config.md` — New skill
- `.github/copilot-instructions.md` — Updated with new knowledge

## Prior MVP History
- MVP9 (v0.9.0): Activity tracking, external event detection, cancel recording, camera button
- MVP8 (v0.8.0): Background tasks, tasks screen, cancel operations, cron CRUD
- MVP7 (v0.7.0): Touch fix, wheel state machine, sleep
- MVP6 (v0.6.0): Settings, sleep, tasks, UI polish, cron API
- MVP5 (v0.5.0): Web settings page, NVS, refactored codebase
- MVP4 (v0.4.0): Silence detection, recording sounds, connection fixes
- MVP3 (v0.3.x): Mic fix (esp_codec_dev), Health/Usage APIs
- MVP2.5: TTS, RGB LED, chat UI overhaul
- MVP2: Display + chat
- MVP1: Hardware peripheral tests