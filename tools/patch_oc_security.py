import re, sys

filepath = "/home/user/.npm-global/lib/node_modules/openclaw/dist/net-COi3RSq7.js"

with open(filepath, "r") as f:
    content = f.read()

# Backup original
with open(filepath + ".bak", "w") as f:
    f.write(content)

# Replace the isSecureWebSocketUrl function to also accept private/RFC1918 IPs
old = 'return isLoopbackHost(parsed.hostname);'
new_code = '''const h = parsed.hostname.trim().toLowerCase();
if (isLoopbackHost(h)) return true;
// Allow RFC1918 private network addresses for LAN usage (HeyClawy patch)
const unbracket = h.startsWith("[") && h.endsWith("]") ? h.slice(1, -1) : h;
const parts = unbracket.split(".").map(Number);
if (parts.length === 4 && parts.every(p => !isNaN(p))) {
  if (parts[0] === 10) return true;
  if (parts[0] === 172 && parts[1] >= 16 && parts[1] <= 31) return true;
  if (parts[0] === 192 && parts[1] === 168) return true;
}
return false;'''

if old not in content:
    print("ERROR: Could not find target string to patch")
    sys.exit(1)

content = content.replace(old, new_code)

with open(filepath, "w") as f:
    f.write(content)

print("OK - Patched isSecureWebSocketUrl to accept RFC1918 private IPs")
