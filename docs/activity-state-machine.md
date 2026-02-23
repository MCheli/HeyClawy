# HeyClawy ↔ OpenClaw Activity State Machine

## OpenClaw Terminology

| Term | Meaning |
|------|---------|
| **Session** | Persistent conversation thread (has key, age, token count, model) |
| **Agent** | AI worker entity — processes messages, uses tools, produces responses |
| **Run** | Single execution of an agent on a message (has `runId`, streams events) |
| **Cron Job** | Scheduled agent execution (has id, schedule, state with `runningAtMs`) |
| **Channel** | Communication pathway: WhatsApp, WebUI, TUI, hooks, HeyClawy |

## WebSocket Events OpenClaw Sends

| Event | Payload | When | Currently Handled? |
|-------|---------|------|--------------------|
| `chat` (state=delta) | Streaming text chunk | During response generation | ✅ Yes (device + external) |
| `chat` (state=final) | Complete response | Response finished | ✅ Yes |
| `chat` (state=error) | Error message | Agent error | ✅ Yes (device only) |
| `chat` (state=aborted) | Abort info | User/timeout aborted | ❌ NO |
| `agent` (stream=lifecycle) | Phase start/end | Agent run start/end | ❌ NO |
| `agent` (stream=tool) | Tool name, args, result | Tool execution | ❌ NO |
| `agent` (stream=assistant) | LLM text output | Thinking/generating | ❌ NO |
| `cron` | Job start/finish/error | Cron execution | ❌ NO |
| `health` | Full health snapshot | Periodic (~30s) | ⚠️ Partial (only ok + channels) |
| `tick` | Timestamp | Keepalive | ✅ Yes |

## Two Use Cases

### Case A: Device-Initiated Request
User presses wheel → records audio → device sends to OpenClaw → waits for response.

### Case B: Externally-Initiated Activity  
Someone (or cron) triggers OpenClaw via WhatsApp, TUI, WebUI, hooks → HeyClawy should detect and display.

## Activity Source Matrix

| Source | Generates `chat` events? | Generates `agent` events? | Generates `cron` events? |
|--------|--------------------------|---------------------------|--------------------------|
| HeyClawy (device) | ✅ Yes | ✅ Yes | N/A |
| WhatsApp | ✅ Yes | ✅ Yes | N/A |
| TUI | ✅ Yes | ✅ Yes | N/A |
| WebUI | ✅ Yes | ✅ Yes | N/A |
| Hooks API | ✅ Yes | ✅ Yes | N/A |
| Cron Job | ✅ Yes (if delivering) | ✅ Yes | ✅ Yes |

## Combined State Machine

### OpenClaw States (as observed by HeyClawy)

```
                    ┌──────────────────────────────────────┐
                    │          OC_IDLE                      │
                    │  No active runs, no cron executing    │
                    │  Screen: "READY" (green)              │
                    │  RGB: solid green                     │
                    │  Info: uptime, last activity Xs ago   │
                    └──────────┬───────────────────┬────────┘
                               │                   │
                 Device wheel  │                   │ External event
                 press+record  │                   │ (chat/agent/cron)
                               │                   │
                    ┌──────────▼──────────┐  ┌─────▼────────────────┐
                    │  DEVICE_RECORDING    │  │   OC_EXT_ACTIVE      │
                    │  Screen: LISTENING   │  │   Screen: "ACTIVE"   │
                    │  RGB: solid blue     │  │   (blue) + detail    │
                    │  Mic capturing audio │  │   RGB: blue breathe  │
                    └──────────┬──────────┘  │   Shows: source,     │
                               │             │   elapsed, tool info  │
                    silence/    │             └─────┬────────────────┘
                    wheel press│                    │
                               │                    │ chat final/
                    ┌──────────▼──────────┐         │ agent lifecycle end
                    │  DEVICE_SENDING     │         │
                    │  Screen: SENDING    │  ┌──────▼─────────────────┐
                    │  STT + chat.send    │  │   OC_EXT_DONE          │
                    │  RGB: blue breathe  │  │   Brief "Done" flash   │
                    └──────────┬──────────┘  │   → return to OC_IDLE  │
                               │             └────────────────────────┘
                    ┌──────────▼──────────┐
                    │  DEVICE_THINKING    │
                    │  Screen: THINKING   │
                    │  Timer running      │
                    │  RGB: orange breathe│
                    │  Can show tool info │
                    │  from agent events  │
                    └──────────┬──────────┘
                               │
                    chat delta │
                               │
                    ┌──────────▼──────────┐
                    │  DEVICE_STREAMING   │
                    │  Screen: "..."      │
                    │  RGB: purple breathe│
                    └──────────┬──────────┘
                               │
                    chat final │
                               │
                    ┌──────────▼──────────┐
                    │  DEVICE_RESPONSE    │
                    │  Screen: response   │
                    │  Buttons: Play/Info │
                    │  60s timeout → IDLE │
                    └──────────┬──────────┘
                               │
                    timeout/    │
                    wheel click │
                               │
                    ┌──────────▼──────────┐
                    │     → OC_IDLE       │
                    └─────────────────────┘
```

### Concurrent Scenarios

#### S1: Device idle, nothing happening
- State: `OC_IDLE`
- Screen: "READY" (green), last activity time, uptime, WA status, cron summary
- RGB: solid green
- Health polled every 30s

#### S2: User initiates voice command
- `OC_IDLE` → `DEVICE_RECORDING` → `DEVICE_SENDING` → `DEVICE_THINKING` → `DEVICE_STREAMING` → `DEVICE_RESPONSE` → `OC_IDLE`
- Full control: can abort at THINKING, cancel at RECORDING
- Timer shown during THINKING

#### S3: External activity while device idle
- `OC_IDLE` → `OC_EXT_ACTIVE` → `OC_IDLE`
- Triggered by: `chat` delta/init event with `chat_cb == NULL`, OR `agent` lifecycle start, OR `cron` event
- Shows: "ACTIVE" + source info + elapsed timer + tool being used
- RGB: blue breathe
- Health polling switches to 5s
- Device wakes from sleep if sleeping

#### S4: External activity while device is in DEVICE_RESPONSE
- Device shows response, external activity starts
- Don't interrupt response display — update status bar/info area only
- After response timeout → show external activity if still ongoing

#### S5: User wants to record while external activity is ongoing
- Wheel press during `OC_EXT_ACTIVE`:
  - **Allow it** — OpenClaw can handle concurrent sessions
  - Transition to `DEVICE_RECORDING`, external activity tracking continues in background
  - After device response is shown, if external is still active, show it again

#### S6: Cron job starts while device idle
- Cron `start` event → `OC_EXT_ACTIVE` with cron job name
- Cron `finish` event → `OC_IDLE`
- If cron runs while device is sleeping → wake device

#### S7: User wants to cancel external activity
- Touch "abort" area or dedicated gesture during `OC_EXT_ACTIVE`
- Send `chat.abort` with the running session key
- This works for ANY source (WhatsApp, TUI, hooks, cron)

#### S8: Device sleeping, external activity starts
- WebSocket `chat`/`agent`/`cron` event arrives
- `external_activity_detected` flag set
- `status_update_task` detects → calls `app_reset_activity_timer()`
- Device wakes, shows `OC_EXT_ACTIVE`

#### S9: Multiple concurrent activities
- OpenClaw supports `maxConcurrent: 4` agent runs
- Show count: "2 Active" instead of "ACTIVE"
- Track via `runId` from events

#### S10: Activity starts and finishes very quickly (< 2s)
- Show brief "Activity" flash (at least 3s display time)
- Then return to IDLE with updated "last: just now"

## What Needs to Change

### 1. Handle ALL WebSocket Events (openclaw_client.c)

Currently only: `connect.challenge`, `chat`, `tick`, `health`

**Add handlers for:**
- `agent` events: Extract `stream` (lifecycle/tool/assistant), `runId`, tool name
- `cron` events: Extract job name, status (start/finish/error)
- `chat` (state=aborted): Handle aborted state

**New info fields needed:**
```c
typedef struct {
    // ... existing fields ...
    
    // Current activity tracking
    bool is_active;              // ANY run in progress (device or external)
    bool is_external;            // Activity is from external source
    char active_source[24];      // "whatsapp", "tui", "hooks", "cron", "device"
    char active_run_id[48];      // Current runId for abort
    char active_session_key[48]; // Session key for abort
    char active_detail[64];      // "Using tool: web_search" or "Thinking..."
    int64_t active_started_ms;   // When activity started (epoch ms for timer)
    int active_count;            // Number of concurrent active runs
} openclaw_info_t;
```

### 2. Real-time Event Processing

Instead of relying on health polling for activity detection:
- Process `agent` lifecycle events for immediate start/end detection
- Process `agent` tool events for "Using: web_search" detail
- Process `cron` events for job start/end
- Keep health polling as fallback/verification (10s during active, 30s idle)

### 3. UI Changes (ui.c)

**ACTIVE state display:**
- Big label: "ACTIVE" or "2 ACTIVE" (blue)
- Sub-label: elapsed timer + detail ("🔧 web_search" or "💭 Thinking")
- Source indicator: small text showing source channel
- Cancel hint if applicable

**IDLE state display:**
- Big label: "READY" (green)
- Sub-label: "Tap or Wheel"
- Info: last activity time, uptime, cron summary
- If cron job running: show job name in task area

### 4. Abort External Operations

- `chat.abort` API: `{type:"req", method:"chat.abort", params:{sessionKey:"..."}}`
- Need to track `sessionKey` from incoming events to know what to abort
- UI: during `OC_EXT_ACTIVE`, touch empty area = abort

### 5. Sleep + Activity Integration

Already implemented (previous fix), but enhance:
- `agent` and `cron` events should also trigger wake (not just `chat` events)
- Health poll fallback during sleep already works at 15s interval

## Event Flow Diagram

```
WebSocket ──┬── chat event ──────── state tracking ──── UI update
            │                              │
            ├── agent event ─── detail ────┤
            │   (tool/lifecycle)           │
            ├── cron event ─── job info ───┤
            │                              │
            └── health event ── fallback ──┘
                                           │
                              ┌────────────▼─────────────┐
                              │  status_update_task       │
                              │  Every 500ms:             │
                              │  - Check activity flags   │
                              │  - Update UI              │
                              │  - Update RGB LED         │
                              │  - Wake from sleep        │
                              │  - Poll health (adaptive) │
                              └───────────────────────────┘
```

## Implementation Priority

1. **Handle `agent` events** — gives real-time tool/thinking info and precise start/end
2. **Handle `cron` events** — gives cron job tracking without polling
3. **Track `runId` and `sessionKey`** — enables abort of external operations  
4. **Elapsed timer for external activity** — use `active_started_ms` from lifecycle start
5. **Show tool/thinking detail** — from agent stream events
6. **Abort external operations** — `chat.abort` with tracked session key
7. **Handle `aborted` chat state** — clean transition on abort
8. **Concurrent activity count** — from tracking multiple `runId`s
