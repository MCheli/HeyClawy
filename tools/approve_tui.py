import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Grant operator.admin scope to a paired OpenClaw device.")
    parser.add_argument("--device-id", required=True, help="Device ID to update in paired.json")
    parser.add_argument(
        "--openclaw-home",
        default="/home/user/.openclaw",
        help="Path to OpenClaw home directory (default: /home/user/.openclaw)",
    )
    args = parser.parse_args()

    base = Path(args.openclaw_home) / "devices"
    paired_path = base / "paired.json"
    pending_path = base / "pending.json"

    with paired_path.open() as f:
        paired = json.load(f)

    did = args.device_id
    if did not in paired:
        raise SystemExit(f"Device ID not found in paired.json: {did}")

    paired[did]["scopes"] = ["operator.admin"]
    if "tokens" in paired[did]:
        for token_name in paired[did]["tokens"]:
            paired[did]["tokens"][token_name]["scopes"] = ["operator.admin"]

    with paired_path.open("w") as f:
        json.dump(paired, f, indent=4)

    with pending_path.open("w") as f:
        json.dump({}, f)

    print(f"OK - approved {did} with operator.admin")


if __name__ == "__main__":
    main()
