import json
import socket
import threading
import time

from aether import coap
from aether.runtime import Runtime
from aether.state import State


def test_real_udp_collector_and_duplicate_cache(monkeypatch):
    monkeypatch.setenv('AETHER_BIND', '127.0.0.1')
    state = State(); runtime = Runtime(state)
    worker = threading.Thread(target=runtime.coap_worker, daemon=True)
    worker.start()
    try:
        time.sleep(.05)
        payload = json.dumps({'system': 'AETHER', 'node': 'nodeB', 'state': 'HEALTHY'}).encode()
        message = coap.Message(0, 2, 0x5152, b'AB', ((11, b'aether'), (11, b'nodeB')), payload)
        raw = coap.encode(message)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(1); sock.connect(('127.0.0.1', 5683))
            sock.send(raw); first = sock.recv(2048)
            assert coap.decode(first).code == 68
            received_at = state.seen['nodeB']
            sock.send(raw); assert sock.recv(2048) == first
            assert state.seen['nodeB'] == received_at
            # Bad JSON shape must not kill the collector thread.
            message.mid += 1; message.payload = b'[]'
            sock.send(coap.encode(message))
        result = coap.request('127.0.0.1', '/3/0')
        assert result['code'] == '2.05' and worker.is_alive()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as bad:
            bad.settimeout(1); bad.connect(('127.0.0.1', 5683))
            bad.send(coap.encode(coap.Message(0, 1, 20, b'', ((5, b''), (11, b'3'), (11, b'0')))))
            assert coap.decode(bad.recv(2048)).code == 130
        assert state.snapshot()['protocols']['COAP_LWM2M']
    finally:
        runtime.stop.set(); worker.join(timeout=2)
        assert not worker.is_alive()
