# Wheel Button State Machine

## Overview
The SenseCAP Watcher has a rotary encoder ("wheel") with:
- **Click** (short press < 4s): context-dependent action
- **Long press** (≥ 4s): deep sleep / power off
- **Spin** (rotate): wake from sleep (future: volume, scroll)

## State Machine

### Device States
| State | Display | RGB LED |
|-------|---------|---------|
| SLEEPING | Off | Off |
| BOOT | "BOOT" | Blue breathing |
| IDLE | "Ready" (green) | Solid green |
| LISTENING | "Listening..." | Solid blue |
| SENDING | "Transcribing..." / "Sending..." | Blue breathing |
| THINKING | Timer + orange | Orange breathing |
| RESPONSE | Response text | Based on response |
| TTS_LOADING | "Loading audio..." | Blue breathing |
| TTS_PLAYING | "Playing..." | Cyan breathing |
| ERROR | Error message | Red blink |

### Wheel Click Actions by State

| Current State | Short Click | Long Press (≥4s) |
|---------------|-------------|------------------|
| **SLEEPING** | Wake up (display on, reconnect) — does NOT start recording | Deep sleep (redundant) |
| **IDLE** | Start recording → LISTENING | Enter deep sleep |
| **LISTENING** | If has speech: stop & send. If no speech: cancel → IDLE | Enter deep sleep |
| **SENDING** | (ignored) | Enter deep sleep |
| **THINKING** | Abort chat → IDLE | Enter deep sleep |
| **RESPONSE** | Start new recording → LISTENING | Enter deep sleep |
| **TTS_LOADING** | (ignored) | Enter deep sleep |
| **TTS_PLAYING** | Stop TTS → IDLE | Enter deep sleep |
| **TASKS_SCREEN** | Back to main screen | Enter deep sleep |
| **ERROR** | (ignored) | Enter deep sleep |

### Wheel Spin Actions by State

| Current State | Spin |
|---------------|------|
| **SLEEPING** | Wake up (display on) |
| **TASKS_SCREEN** | Scroll task list (up/down) |
| **All other** | Reset activity timer (future: volume control) |

### Touch Actions by State

| Current State | Touch Screen | Touch "Ready" Label |
|---------------|-------------|---------------------|
| **SLEEPING** | Wake up | Wake up |
| **IDLE** | Tick sound + reset timer | Start recording → LISTENING |
| **RESPONSE** | Tick sound | (no action — label shows response) |
| **All other** | Tick sound + reset timer | (no action) |

### Button Bar Actions (Touch)

| Button | Icon | Action |
|--------|------|--------|
| **Play** | ▶ | TTS play current response |
| **Details** | ℹ | Request detailed response from OpenClaw |
| **Web** | 📶 | Toggle web server on/off |
| **Tasks** | ☰ | Toggle Tasks screen |

### Tasks Screen Touch Actions

| Element | Action |
|---------|--------|
| **Task row** | Toggle enabled/disabled (shows detail panel) |
| **Empty area** | Back to main screen |
| **Detail panel** | Dismiss panel |

## Sleep Flow

```
IDLE ──(timeout)──> LIGHT SLEEP (display off, LED off, CPU running)
                         │
                    ┌────┴────┐
                    │  Knob   │──> IDLE (wake, don't record)
                    │  Touch  │──> IDLE (wake)
                    │  Spin   │──> IDLE (wake)
                    └────┬────┘
                         │
                  (3x timeout)
                         │
                         v
                    DEEP SLEEP (CPU off, GPIO2 wake only)
                         │
                    ┌────┴────┐
                    │ Knob btn│──> REBOOT → BOOT → IDLE
                    └─────────┘
```

## Future Features (Not Yet Implemented)
- **Double-click**: Quick action (e.g., repeat last command)
- **Spin during RESPONSE**: Scroll through long response text
