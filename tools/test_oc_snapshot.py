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

snapshot = resp.get("payload", {}).get("snapshot", {})
print("authMode:", snapshot.get("authMode"))
print("uptimeMs:", snapshot.get("uptimeMs"))

health = snapshot.get("health", {})
print("\nHealth:")
print(json.dumps(health, indent=2)[:2000])

presence = snapshot.get("presence", {})
print("\nPresence:")
print(json.dumps(presence, indent=2)[:1000])

session_defaults = snapshot.get("sessionDefaults", {})
print("\nSession defaults:")
print(json.dumps(session_defaults, indent=2)[:500])

# Also check health events pushed by server
print("\n--- Waiting for health event ---")
for _ in range(10):
    try:
        msg = json.loads(ws.recv())
        if msg.get("event") == "health":
            h = msg.get("payload", {})
            print("Health event ok:", h.get("ok"))
            print("Duration:", h.get("durationMs"), "ms")
            channels = h.get("channels", {})
            for name, ch in channels.items():
                status = "connected" if ch.get("connected") else "disconnected" if ch.get("configured") else "off"
                print(f"  Channel {name}: {status}")
            agents = h.get("agents", {})
            if isinstance(agents, dict):
                for name, ag in agents.items():
                    print(f"  Agent {name}: {ag}")
            elif isinstance(agents, list):
                for ag in agents:
                    print(f"  Agent: {ag}")
            break
    except:
        break

ws.close()
