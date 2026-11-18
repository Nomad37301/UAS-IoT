import json
import struct
import time
from types import SimpleNamespace

import pytest
from fastapi.testclient import TestClient

from aether.app import create_app
from aether.state import State
from aether import coap, rtu
from aether.runtime import Runtime


class FakeRuntime:
    def __init__(self): self.connected = True
    def publish_mode(self): return self.connected
    def actuator(self, target):
        if target == 'COAP_POLL': return {'/3/0': {'code': '2.05'}}
        return {'queued_to_broker': self.connected, 'executed': False}


@pytest.fixture
def pair():
    state, runtime = State(), FakeRuntime()
    state.host.update(cpu_temp_c=49.2, service_running=True)
    with TestClient(create_app(state, runtime, start_workers=False)) as client:
        yield client, state, runtime


def test_health_faults_and_recovery_visibility(pair):
    client, state, _ = pair
    assert client.get('/health?probe=B').status_code == 200
    assert client.get('/health?probe=C').status_code == 200
    assert client.post('/api/v1/fault/inject', json={'mode': 'EXT_BLOCK'}).status_code == 200
    assert client.get('/health?probe=B').status_code == 200
    assert client.get('/health?probe=C').status_code == 403
    client.post('/api/v1/fault/inject', json={'mode': 'SERVICE_CRASH'})
    assert client.get('/health?probe=B').status_code == 503
    # API/dashboard survive the simulated target service failure.
    assert client.get('/api/v1/status').status_code == 200
    assert 'AETHER' in client.get('/').text
    assert client.post('/api/v1/fault/inject', json={'mode': 'bad'}).status_code == 422
    assert client.get('/api/v1/incidents').json()[0]['diagnosis'] == 'SERVICE_CRASH'


def test_authentication_and_broker_failure(pair, monkeypatch):
    client, _, runtime = pair
    monkeypatch.setenv('AETHER_API_KEY', 'unit-secret')
    assert client.post('/api/v1/actuator/cmd', json={'target': 'MUTE_ALARM'}).status_code == 401
    headers = {'Authorization': 'Bearer unit-secret'}
    assert client.post('/api/v1/actuator/cmd', json={'target': 'MUTE_ALARM'}, headers=headers).json()['executed'] is False
    runtime.connected = False
    assert client.post('/api/v1/actuator/cmd', json={'target': 'RELAY_RECOVERY'}, headers=headers).status_code == 503
    assert client.post('/api/v1/actuator/cmd', json={'target': 'bad'}, headers=headers).status_code == 422
    assert client.get('/api/v1/incidents?limit=1001').status_code == 422


def test_telemetry_freshness_and_isolation():
    state = State()
    state.ingest('nodeB', {'system': 'AETHER', 'node': 'nodeB', 'state': 'HEALTHY', 'http_code': 200})
    state.ingest('nodeC', {'system': 'AETHER', 'node': 'nodeC', 'http_ext_code': 403})
    assert state.snapshot()['state'] == 'EXT_ISOLATED'
    state.ingest('nodeC', {'system': 'AETHER', 'node': 'nodeC', 'http_ext_code': 200, 'rf_rx_alive': False})
    assert state.snapshot()['diagnosis'] == 'RADIO_LINK_UNAVAILABLE'
    state.seen['nodeC'] -= 11
    assert state.snapshot()['diagnosis'] == 'EXTERNAL_PROBE_STALE'
    state.seen['nodeB'] -= 11
    assert state.snapshot()['diagnosis'] == 'WATCHDOG_TELEMETRY_STALE'
    with pytest.raises(ValueError): state.ingest('nodeB', {'system': 'wrong', 'node': 'nodeB'})
    with pytest.raises(ValueError): state.ingest('nodeB', {'system': 'AETHER', 'node': 'nodeB', 'state': 'HEALTHY', 'http_code': True})


def test_persistent_bounded_incidents(tmp_path):
    database = str(tmp_path / 'incidents.sqlite')
    state = State(database)
    for index in range(1005): state.incident('test', str(index), 'evidence', 'NONE')
    restored = State(database)
    assert len(restored.incidents(1000)) == 1000
    assert restored.incidents(1)[0]['diagnosis'] == '1004'


def test_modbus_read_registers_and_write():
    words = [492, 34, 20, 1, 65535]
    restart = []
    read = bytes.fromhex('01030000000585c9')
    reply = rtu.response(read, lambda: words, lambda source: restart.append(source) or True)
    assert reply == rtu.frame(bytes.fromhex('01030a01ec002200140001ffff'))
    assert rtu.crc16(bytes.fromhex('01030000000a')) == 0xcdc5
    write = bytes.fromhex('010600050001580b')
    assert rtu.response(write, lambda: words, lambda source: restart.append(source) or True) == write
    assert restart == ['MODBUS']
    assert rtu.response(write, lambda: words, lambda source: False) == rtu.frame(bytes((1, 0x86, 6)))
    assert rtu.response(read, lambda: None, lambda source: True) == rtu.frame(bytes((1, 0x83, 4)))
    for body, exception in ((bytes.fromhex('010300040002'), 2), (bytes.fromhex('010600050002'), 3), (bytes.fromhex('010400000001'), 1)):
        assert rtu.response(rtu.frame(body), lambda: words, lambda source: True)[2] == exception
    assert rtu.response(read[:-1] + b'\x00', lambda: words, lambda source: True) is None
    assert rtu.response(rtu.frame(bytes.fromhex('000300000005')), lambda: words, lambda source: True) is None


def test_runtime_register_contract_and_recovery_cooldown():
    state = State(); runtime = Runtime(state)
    assert runtime.registers() is None
    state.host.update(cpu_temp_c=49.2, ram_pct=34, load_pct=20, service_running=True, uptime_s=65536)
    assert runtime.registers() == [492, 34, 20, 1, 0]
    state.mode = 'SERVICE_CRASH'
    assert runtime.registers()[3] == 0
    assert runtime.recovery('GPIO') is True
    assert runtime.recovery('MODBUS') is False


def test_coap_wire_and_malformed_frames():
    packet = coap.Message(0, 1, 0x1234, b'AB', ((11, b'3303'), (11, b'0'), (11, b'5700')))
    wire = coap.encode(packet)
    assert wire.hex() == '420112344142b43333303301300435373030'
    assert coap.decode(wire).path == '/3303/0/5700'
    reply = coap.Message(2, 69, packet.mid, packet.token, ((12, b'\x32'),), b'{"v":5.0}')
    assert coap.decode(coap.encode(reply)).payload == b'{"v":5.0}'
    for data in (b'', b'\x40\x01', b'\x49\x01\x00\x00', b'\x40\x01\x00\x00\xff', b'\x40\x01\x00\x00\xf0'):
        with pytest.raises(ValueError): coap.decode(data)


def test_mqtt_callback_identity_and_incidents():
    state = State(); runtime = Runtime(state)
    payload = {'system': 'AETHER', 'node': 'nodeB', 'state': 'SERVICE_DOWN', 'http_code': 503}
    runtime.on_message(None, None, SimpleNamespace(topic='aether/nodeB/telemetry', payload=json.dumps(payload).encode()))
    assert state.snapshot()['state'] == 'SERVICE_DOWN'
    runtime.on_message(None, None, SimpleNamespace(topic='aether/nodeB/telemetry', payload=b'{"system":"AETHER","node":"nodeC"}'))
    assert state.nodes['nodeB']['state'] == 'SERVICE_DOWN'
    runtime.on_message(None, None, SimpleNamespace(topic='aether/alert/incident', payload=b'{"source":"nodeB","diagnosis":"POWER_FAULT"}'))
    assert state.incidents()[0]['diagnosis'] == 'POWER_FAULT'


def test_rf_contract_golden():
    packet = struct.pack('<BBBBffffHHB7s', 0xae, 1, 2, 15, 5.0, 37.5, 25, 50, 200, 0x1234, 0, bytes(7))
    assert len(packet) == 32
    assert packet.hex() == 'ae01020f0000a040000016420000c84100004842c80034120000000000000000'
