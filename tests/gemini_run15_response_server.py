from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

PAYLOAD = Path("generated-database-first-gemini/database_first_run_65434_15/gemini_response.json").read_bytes()

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        self.rfile.read(n)
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(PAYLOAD)))
        self.end_headers()
        self.wfile.write(PAYLOAD)
    def log_message(self, *_):
        pass

HTTPServer(("127.0.0.1", 8767), Handler).serve_forever()
