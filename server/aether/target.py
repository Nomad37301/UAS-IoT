"""Independent target service, restarted by the recovery daemon."""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != '/health':
            self.send_error(404); return
        body = json.dumps({'system': 'AETHER', 'status': 'ok'}).encode()
        self.send_response(200); self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)


if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', 8001), Handler).serve_forever()
