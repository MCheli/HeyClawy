"""Load secrets from secrets.txt in project root."""
import os

def load_secrets():
    """Parse secrets.txt and return dict of key=value pairs."""
    secrets_path = os.path.join(os.path.dirname(__file__), '..', 'secrets.txt')
    if not os.path.exists(secrets_path):
        raise FileNotFoundError(
            f"secrets.txt not found at {secrets_path}\n"
            "Copy secrets_example.txt to secrets.txt and fill in your values."
        )
    cfg = {}
    with open(secrets_path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                k, v = line.split('=', 1)
                cfg[k.strip()] = v.strip()
    return cfg

if __name__ == '__main__':
    import json
    print(json.dumps(load_secrets(), indent=2))
