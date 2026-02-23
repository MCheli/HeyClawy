import websocket, json
from load_secrets import load_secrets

cfg = load_secrets()
ws = websocket.WebSocket()
ws.connect(f"ws://{cfg['OC_HOST']}:{cfg['OC_PORT']}")
ws.settimeout(5)

msg = json.loads(ws.recv())

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
# Print allowed scopes from response
hello_ok = resp.get("payload", {})
print("Response payload keys:", list(hello_ok.keys()))
print("Scopes granted:", hello_ok.get("scopes", "n/a"))

# List available methods
methods = ["usage.status", "usage.cost", "sessions.list", "sessions.usage", 
           "system.status", "system.info", "agent.status"]
for m in methods:
    ws.send(json.dumps({"type": "req", "id": m, "method": m, "params": {}}))

# Collect responses
for _ in range(20):
    try:
        msg = json.loads(ws.recv())
        if msg.get("type") == "res":
            mid = msg.get("id", "?")
            ok = msg.get("ok", False)
            err = msg.get("error", {}).get("message", "")
            payload = msg.get("payload", {})
            if ok:
                keys = list(payload.keys()) if isinstance(payload, dict) else str(payload)[:200]
                print(f"  {mid}: OK keys={keys}")
                if payload:
                    print(f"    data: {json.dumps(payload)[:500]}")
            else:
                print(f"  {mid}: FAIL: {err}")
        elif msg.get("type") == "event":
            ev = msg.get("event", "?")
            if ev == "health":
                h = msg.get("payload", {})
                chans = h.get("channels", {})
                agents = h.get("agents", {})
                if isinstance(agents, dict):
                    for name, ag in agents.items():
                        print(f"  Agent {name}: {ag}")
    except:
        break

ws.close()
