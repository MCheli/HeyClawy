import websocket, json
from load_secrets import load_secrets

cfg = load_secrets()
ws = websocket.WebSocket()
ws.connect(f"ws://{cfg['OC_HOST']}:{cfg['OC_PORT']}")
ws.settimeout(5)

msg = json.loads(ws.recv())
nonce = msg["payload"]["nonce"]

connect_req = {
    "type": "req", "id": "1", "method": "connect",
    "params": {
        "minProtocol": 3, "maxProtocol": 3,
        "client": {"id": "test", "version": "1.0", "platform": "test", "mode": "cli"},
        "role": "operator",
        "scopes": ["operator.read", "operator.write"],
        "auth": {"token": cfg['OC_TOKEN']}
    }
}
ws.send(json.dumps(connect_req))
resp = json.loads(ws.recv())
print("Connect:", "OK" if resp.get("ok") else "FAIL")

# Try usage.cost
ws.send(json.dumps({"type": "req", "id": "4", "method": "usage.cost", "params": {
    "from": "2026-01-01", "to": "2026-12-31"
}}))
for _ in range(10):
    msg = json.loads(ws.recv())
    if msg.get("type") == "res" and msg.get("id") == "4":
        print("Cost:", json.dumps(msg, indent=2)[:2000])
        break

# sessions.list
ws.send(json.dumps({"type": "req", "id": "5", "method": "sessions.list", "params": {}}))
for _ in range(10):
    msg = json.loads(ws.recv())
    if msg.get("type") == "res" and msg.get("id") == "5":
        payload = msg.get("payload", {})
        sessions = payload.get("sessions", [])
        for s in sessions[:3]:
            print(f"Session: {s.get('key', '?')} msgs={s.get('messageCount', '?')}")
        break

# Capture health event
try:
    for _ in range(5):
        msg = json.loads(ws.recv())
        if msg.get("event") == "health":
            h = msg.get("payload", {})
            print(f"Health: ok={h.get('ok')}")
            chans = h.get("channels", {})
            for name, ch in chans.items():
                print(f"  {name}: configured={ch.get('configured')} connected={ch.get('connected')}")
            agents = h.get("agents", {})
            for name, ag in agents.items():
                print(f"  Agent {name}: status={ag.get('status')} lastSeen={ag.get('lastSeen', '?')}")
            break
except Exception as e:
    print(f"Health timeout: {e}")

ws.close()
