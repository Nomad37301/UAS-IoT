from __future__ import annotations

import copy
import json
import math
import sqlite3
import threading
import time
from pathlib import Path

STATES = ('HEALTHY', 'EXT_ISOLATED', 'SERVICE_DOWN', 'NETWORK_DEAD', 'HW_ANOMALY')


def clean(value):
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, dict):
        return {str(k): clean(v) for k, v in value.items()}
    if isinstance(value, (tuple, list)):
        return [clean(v) for v in value]
    return value


class State:
    def __init__(self, database: str = ':memory:'):
        if database != ':memory:':
            Path(database).parent.mkdir(parents=True, exist_ok=True)
        self.lock = threading.RLock()
        self.db = sqlite3.connect(database, check_same_thread=False)
        self.db.execute('CREATE TABLE IF NOT EXISTS incidents (id INTEGER PRIMARY KEY, timestamp REAL, body TEXT)')
        self.nodes = {}
        self.seen = {}
        self.mode = 'NORMAL'
        self.host = {'cpu_temp_c': None, 'ram_pct': 0, 'load_pct': 0, 'uptime_s': 0, 'service_running': False}
        self.links = {'mqtt': False, 'uart': False, 'gpio': False}
        self.coap_seen = 0.0
        self.modbus_seen = 0.0

    def ingest(self, node: str, data: dict):
        if not isinstance(data, dict) or node not in ('nodeB', 'nodeC') or data.get('system') != 'AETHER' or data.get('node') != node:
            raise ValueError('invalid telemetry identity')
        if node == 'nodeB' and data.get('state') not in STATES:
            raise ValueError('invalid AETHER state')
        # Reject booleans posing as integers, invalid ranges, and unbounded payloads.
        for key in ('http_code', 'http_ext_code'):
            if key in data and (type(data[key]) is not int or not 0 <= data[key] <= 599):
                raise ValueError('invalid HTTP code')
        if len(json.dumps(data, allow_nan=False)) > 4096:
            raise ValueError('telemetry too large')
        with self.lock:
            self.nodes[node] = clean(copy.deepcopy(data))
            self.seen[node] = time.monotonic()

    def incident(self, source: str, diagnosis: str, evidence: str, action: str):
        body = dict(timestamp=time.time(), source=source, diagnosis=diagnosis[:96],
                    evidence=evidence[:512], action=action[:128])
        with self.lock:
            self.db.execute('INSERT INTO incidents(timestamp,body) VALUES (?,?)',
                            (body['timestamp'], json.dumps(body, allow_nan=False)))
            self.db.execute('DELETE FROM incidents WHERE id NOT IN (SELECT id FROM incidents ORDER BY id DESC LIMIT 1000)')
            self.db.commit()
        return body

    def incidents(self, limit: int = 100):
        with self.lock:
            rows = self.db.execute('SELECT id,body FROM incidents ORDER BY id DESC LIMIT ?', (limit,)).fetchall()
            return [dict(id=i, **json.loads(body)) for i, body in rows]

    def snapshot(self):
        now = time.monotonic()
        with self.lock:
            nodes = copy.deepcopy(self.nodes)
            ages = {node: round(now - received, 2) for node, received in self.seen.items()}
            b, c = nodes.get('nodeB', {}), nodes.get('nodeC', {})
            b_live, c_live = ages.get('nodeB', math.inf) < 10, ages.get('nodeC', math.inf) < 10
            state = b.get('state', 'NETWORK_DEAD') if b_live else 'NETWORK_DEAD'
            diagnosis = b.get('diagnosis', state) if b_live else 'WATCHDOG_TELEMETRY_STALE'
            if b_live and state == 'HEALTHY' and c_live and b.get('http_code') == 200 and c.get('http_ext_code') != 200:
                state, diagnosis = 'EXT_ISOLATED', 'EXTERNAL_NETWORK_ISOLATION'
            elif b_live and state == 'HEALTHY' and not c_live:
                state, diagnosis = 'EXT_ISOLATED', 'EXTERNAL_PROBE_STALE'
            elif b_live and state == 'HEALTHY' and c_live and c.get('rf_rx_alive') is not True:
                state, diagnosis = 'NETWORK_DEAD', 'RADIO_LINK_UNAVAILABLE'
            protocols = {
                'GPIO': self.links['gpio'], 'UART': self.links['uart'],
                'MODBUS_RTU': now - self.modbus_seen < 10,
                'I2C': b_live and b.get('sensor_ok') is True,
                'SPI_NRF24': c_live and c.get('rf_rx_alive') is True,
                'HTTP': b_live and b.get('http_code') == 200,
                'REST_API': True, 'MQTT': self.links['mqtt'],
                'COAP_LWM2M': now - self.coap_seen < 30,
            }
            return clean(dict(system='AETHER', nodeA=copy.deepcopy(self.host), nodes=nodes,
                              age_s=ages, state=state, diagnosis=diagnosis, fault_mode=self.mode,
                              protocols=protocols, coap_profile='LwM2M/IPSO resource subset'))
