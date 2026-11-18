import io
import json
import threading
import time
import urllib.error
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from types import SimpleNamespace

import pytest
from fastapi.testclient import TestClient

from aether.app import create_app
from aether.influx import InfluxTelemetry, line_protocol
from aether.runtime import Runtime
from aether.state import State


def test_line_protocol_types_and_injection():
    line = line_protocol('nodeB', dict(voltage_v=5, http_code=True, ir_obj_c=float('nan'),
        cpu_temp_c=float('inf'), sensor_ok=True, state='HEALTHY',
        diagnosis='quote" path\\\nnew\rline\x00', untrusted='ignored'), 123)
    assert line.startswith('telemetry,node=nodeB received=1.0,voltage_v=5.0,')
    assert 'sensor_ok=true' in line and 'state="HEALTHY"' in line
    assert 'http_code=' not in line and 'ir_obj_c=' not in line and 'cpu_temp_c=' not in line
    assert 'untrusted' not in line and '\n' not in line and '\r' not in line and '\x00' not in line
    assert 'quote\\" path\\\\ new line' in line
    assert line.endswith(' 123')
    with pytest.raises(ValueError): line_protocol('nodeB,evil=x', {}, 123)


@pytest.mark.parametrize('url,token,database', [
    ('http://[broken', 'secret', 'aether'), ('file:///tmp/db', 'secret', 'aether'),
    ('http://localhost:99999', 'secret', 'aether'),
    ('http://user:password@localhost:8181', 'secret', 'aether'),
    ('http://localhost:8181', '', 'aether'),
    ('http://localhost:8181', 'CHANGE_ME_INFLUX_TOKEN', 'aether'),
    ('http://localhost:8181', 'secret', 'aether;DROP'),
])
def test_bad_configuration_does_not_break_runtime(url, token, database):
    archive = InfluxTelemetry(url, token, database, True)
    assert not archive.enqueue('nodeB', {})
    assert archive.snapshot()['status'] == 'not_configured'


def test_retry_preserves_timestamp_and_bounds_queue():
    requests = []
    def open_request(request, timeout):
        requests.append(request)
        if len(requests) == 1: raise urllib.error.URLError('offline')
        return io.BytesIO(b'')
    archive = InfluxTelemetry('http://localhost:8181', 'unit-token', enabled=True, opener=open_request)
    archive.CAPACITY = 2
    assert archive.enqueue('nodeB', {'voltage_v': 5}, 123)
    assert archive.enqueue('nodeC', {}, 124)
    assert not archive.enqueue('nodeA', {}, 125)
    assert not archive.flush_once()
    assert archive.snapshot()['queued'] == 2
    assert archive.flush_once()
    assert requests[0].data == requests[1].data
    query = urllib.parse.parse_qs(urllib.parse.urlsplit(requests[1].full_url).query)
    assert query == dict(db=['aether'], precision=['nanosecond'], accept_partial=['false'], no_sync=['false'])
    assert requests[1].get_header('Authorization') == 'Bearer unit-token'
    status = archive.snapshot()
    assert status['written'] == 2 and status['dropped'] == 1 and status['queued'] == 0
    assert status['last_error'] is None and 'unit-token' not in json.dumps(status)


@pytest.mark.parametrize('code,queued,dropped', [(401, 1, 0), (429, 1, 0), (503, 1, 0), (400, 0, 1), (422, 0, 1)])
def test_write_failures_and_poison_batch(code, queued, dropped):
    def fail(request, timeout):
        raise urllib.error.HTTPError(request.full_url, code, 'error', {}, io.BytesIO(b'sensitive-server-body'))
    archive = InfluxTelemetry('http://localhost:8181', 'unit-token', enabled=True, opener=fail)
    archive.enqueue('nodeB', {})
    assert not archive.flush_once()
    status = archive.snapshot()
    assert status['queued'] == queued and status['dropped'] == dropped
    assert status['last_error'] == f'HTTP_{code}'


def test_mqtt_ingestion_is_nonblocking_and_incidents_remain_sqlite(monkeypatch, tmp_path):
    monkeypatch.setenv('AETHER_INFLUX_ENABLED', '1')
    monkeypatch.setenv('AETHER_INFLUX_TOKEN', 'unit-token')
    state = State(str(tmp_path / 'incidents.sqlite'))
    runtime = Runtime(state)
    runtime.influx.open = lambda *args, **kwargs: pytest.fail('MQTT callback must not perform HTTP IO')
    data = dict(system='AETHER', node='nodeB', state='HEALTHY', voltage_v=5)
    message = SimpleNamespace(topic='aether/nodeB/telemetry', payload=json.dumps(data).encode())
    runtime.on_message(None, None, message)
    runtime.ingest_telemetry('nodeB', data)  # Duplicate via CoAP within a second.
    assert runtime.influx.snapshot()['queued'] == 1
    message.payload = b'{"system":"WRONG","node":"nodeB"}'
    runtime.on_message(None, None, message)
    assert runtime.influx.snapshot()['queued'] == 1
    runtime.on_message(None, None, SimpleNamespace(topic='aether/alert/incident',
        payload=b'{"source":"nodeB","diagnosis":"SERVICE_DOWN"}'))
    assert State(str(tmp_path / 'incidents.sqlite')).incidents()[0]['diagnosis'] == 'SERVICE_DOWN'


def test_real_http_worker_history_and_api(monkeypatch):
    received = []
    mode = {'code': 204}
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            body = self.rfile.read(int(self.headers['Content-Length']))
            received.append((self.path, self.headers.get('Authorization'), body))
            if self.path == '/api/v3/query_sql':
                result = json.dumps([{'time': '2026-11-19T00:00:00Z', 'voltage_v': 5.01}]).encode()
                self.send_response(200); self.end_headers(); self.wfile.write(result)
            else:
                self.send_response(mode['code']); self.end_headers()
        def log_message(self, *args): pass
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    http_thread = threading.Thread(target=server.serve_forever, daemon=True); http_thread.start()
    stop = threading.Event()
    archive = InfluxTelemetry(f'http://127.0.0.1:{server.server_port}', 'unit-token', enabled=True)
    writer = threading.Thread(target=archive.run, args=(stop,), daemon=True)
    try:
        archive.enqueue('nodeB', {'voltage_v': 5.01}, 1234567890000000000)
        writer.start()
        deadline = time.monotonic() + 4
        while archive.snapshot()['written'] == 0 and time.monotonic() < deadline: stop.wait(.02)
        assert archive.snapshot()['written'] == 1
        assert received[0][1] == 'Bearer unit-token'
        assert received[0][2].endswith(b'1234567890000000000')
        runtime = Runtime(State()); runtime.influx = archive
        monkeypatch.setenv('AETHER_API_KEY', 'api-unit-key')
        with TestClient(create_app(runtime.state, runtime, start_workers=False)) as client:
            headers = {'Authorization': 'Bearer api-unit-key'}
            assert client.get('/api/v1/storage').json()['telemetry']['written'] == 1
            assert client.get('/api/v1/telemetry/history').status_code == 401
            result = client.get('/api/v1/telemetry/history?node=nodeB&minutes=60', headers=headers)
            assert result.status_code == 200 and result.json()['points'][0]['voltage_v'] == 5.01
            query = json.loads(received[-1][2])
            assert query['db'] == 'aether' and "node = 'nodeB'" in query['q'] and 'LIMIT 2000' in query['q']
            assert client.get('/api/v1/telemetry/history?node=evil', headers=headers).status_code == 422
            assert client.get('/api/v1/telemetry/history?minutes=1441', headers=headers).status_code == 422
            archive.configured = False
            assert client.get('/api/v1/telemetry/history', headers=headers).status_code == 503
            assert client.get('/api/v1/status').status_code == 200
            runtime.state.incident('test', 'INFLUX_DOWN', '', 'NONE')
            assert client.get('/api/v1/incidents').json()[0]['diagnosis'] == 'INFLUX_DOWN'
    finally:
        stop.set()
        if writer.is_alive(): writer.join(timeout=3)
        server.shutdown(); server.server_close(); http_thread.join(timeout=2)
        assert not writer.is_alive()
