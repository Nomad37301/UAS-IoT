import threading
import time
from http.server import ThreadingHTTPServer

from aether.runtime import Runtime
from aether.state import State
from aether.target import Handler


def test_real_independent_target_probe(monkeypatch):
    target = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server = threading.Thread(target=target.serve_forever, daemon=True)
    server.start()
    monkeypatch.setenv('AETHER_TARGET_URL', f'http://127.0.0.1:{target.server_port}/health')
    monkeypatch.setenv('AETHER_SIMULATE_HARDWARE', '1')
    state = State(); runtime = Runtime(state)
    worker = threading.Thread(target=runtime.host_worker, daemon=True)
    worker.start()
    try:
        deadline = time.monotonic() + 3
        while not state.host['service_running'] and time.monotonic() < deadline: time.sleep(.02)
        assert state.host['service_running'] and state.host['simulated']
        assert runtime.registers()[3] == 1
        target.shutdown(); target.server_close()
        deadline = time.monotonic() + 3
        while state.host['service_running'] and time.monotonic() < deadline: time.sleep(.02)
        assert not state.host['service_running']
    finally:
        runtime.stop.set(); worker.join(timeout=2)
        target.shutdown(); target.server_close(); server.join(timeout=2)
