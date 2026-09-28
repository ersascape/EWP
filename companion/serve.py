import http.server
import ssl
import subprocess
import os
import sys

PORT = 8443
DIR = os.path.dirname(os.path.abspath(__file__))

cert_file = os.path.join(DIR, "cert.pem")
key_file = os.path.join(DIR, "key.pem")

if not os.path.exists(cert_file) or not os.path.exists(key_file):
    print("Generating self-signed certificate for local HTTPS...")
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048",
        "-keyout", key_file, "-out", cert_file,
        "-days", "365", "-nodes", "-subj", "/CN=ErsaCompanion"
    ], check=True)

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIR, **kwargs)

server = http.server.HTTPServer(("0.0.0.0", PORT), Handler)
ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_ctx.load_cert_chain(certfile=cert_file, keyfile=key_file)
server.socket = ssl_ctx.wrap_socket(server.socket, server_side=True)

print(f"Ersa Web Bluetooth Companion running on HTTPS at port {PORT}")
print(f"Open https://<your-computer-ip>:{PORT} or https://localhost:{PORT}")
try:
    server.serve_forever()
except KeyboardInterrupt:
    print("\nShutting down server.")
    server.server_close()
