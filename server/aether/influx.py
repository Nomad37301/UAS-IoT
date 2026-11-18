"""InfluxDB 3 Core telemetry archive; SQLite remains the incident store.

Only the writer thread performs network writes. Its bounded, volatile queue keeps
MQTT, UART and recovery independent of database availability.
"""
from __future__ import annotations

from collections import deque
import json
import logging
import math
import os
import re
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

LOG = logging.getLogger('aether.influx')
NUMERIC = ('voltage_v', 'ir_obj_c', 'ir_amb_c', 'cpu_temp_c', 'cpu_ram_pct',
           'cpu_load_pct', 'http_code', 'http_lat_ms', 'http_ext_code',
           'http_ext_lat_ms', 'uptime_ms', 'last_rf_seq', 'spi_rf_seq',
           'auto_recovery_attempts', 'state_code', 'uptime_s_low16')
BOOLEAN = ('sensor_ok', 'voltage_ok', 'modbus_ok', 'wifi', 'relay_active',
           'muted', 'rf_rx_alive', 'service_running', 'simulated', 'demo_overheat')
TEXT = ('state', 'diagnosis')
NODES = ('nodeA', 'nodeB', 'nodeC')


def line_protocol(node, data, timestamp_ns):
    if node not in NODES:
        raise ValueError('invalid node')
    # Always float for numeric metrics, even when JSON sends an integer. This
    # prevents Influx schema conflicts when firmware changes 5 to 5.01.
    fields = ['received=1.0']
    for name in NUMERIC:
        value = data.get(name)
        if type(value) in (int, float):
            try: number = float(value)
            except OverflowError: continue
            if math.isfinite(number): fields.append(f'{name}={number!r}')
    for name in BOOLEAN:
        if type(data.get(name)) is bool:
            fields.append(f'{name}={str(data[name]).lower()}')
    for name in TEXT:
        value = data.get(name)
        if isinstance(value, str):
            value = value[:96].replace('\\', '\\\\').replace('"', '\\"')
            value = value.replace('\r', ' ').replace('\n', ' ').replace('\x00', '')
            fields.append(f'{name}="{value}"')
    return f'telemetry,node={node} {",".join(fields)} {int(timestamp_ns)}'


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        # Do not forward the database token to a redirected host.
        return None


class InfluxTelemetry:
    CAPACITY = 1024
    BATCH_SIZE = 32

    def __init__(self, url='', token='', database='aether', enabled=False, opener=None):
        self.url, self.token, self.database = url.rstrip('/'), token, database
        self.enabled = enabled
        self.lock = threading.Lock()
        self.pending = deque()
        self.written = self.dropped = 0
        self.last_write = None
        self.last_error = None
        self.status = 'disabled' if not enabled else 'waiting'
        self.configured = False
        self.open = opener or urllib.request.build_opener(NoRedirect()).open
        if enabled:
            try:
                parsed = urllib.parse.urlsplit(self.url)
                parsed.port  # Reject invalid/out-of-range ports at configuration time.
            except ValueError:
                parsed = urllib.parse.urlsplit('')
            self.configured = bool(parsed.scheme in ('http', 'https') and parsed.hostname
                                   and not parsed.username and not parsed.password
                                   and not parsed.query and not parsed.fragment
                                   and parsed.path in ('', '/') and token
                                   and all(33 <= ord(char) <= 126 for char in token)
                                   and token != 'CHANGE_ME_INFLUX_TOKEN'
                                   and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', database))
            if not self.configured:
                self.status, self.last_error = 'not_configured', 'CHECK_INFLUX_ENV'

    @classmethod
    def from_env(cls):
        return cls(os.getenv('AETHER_INFLUX_URL', 'http://127.0.0.1:8181'),
                   os.getenv('AETHER_INFLUX_TOKEN', ''),
                   os.getenv('AETHER_INFLUX_DATABASE', 'aether'),
                   os.getenv('AETHER_INFLUX_ENABLED', '0') == '1')

    def enqueue(self, node, data, timestamp_ns=None):
        if not self.enabled or not self.configured: return False
        line = line_protocol(node, data, time.time_ns() if timestamp_ns is None else timestamp_ns)
        with self.lock:
            if len(self.pending) >= self.CAPACITY:
                self.dropped += 1
                return False
            self.pending.append(line)
        return True

    def snapshot(self):
        with self.lock:
            return dict(engine='influxdb3', enabled=self.enabled, configured=self.configured,
                        status=self.status, queued=len(self.pending), written=self.written,
                        dropped=self.dropped, last_write=self.last_write, last_error=self.last_error)

    def request(self, path, payload, content_type):
        request = urllib.request.Request(self.url + path, data=payload, method='POST',
                    headers={'Authorization': 'Bearer ' + self.token, 'Content-Type': content_type,
                             'Accept': 'application/json'})
        with self.open(request, timeout=2) as response:
            # Bound the response even if a server is misconfigured.
            body = response.read(2 * 1024 * 1024 + 1)
            if len(body) > 2 * 1024 * 1024: raise ValueError('response too large')
            return body

    def flush_once(self):
        with self.lock: batch = list(self.pending)[:self.BATCH_SIZE]
        if not batch: return True
        permanent = False
        try:
            query = urllib.parse.urlencode({'db': self.database, 'precision': 'nanosecond',
                                            'accept_partial': 'false', 'no_sync': 'false'})
            self.request('/api/v3/write_lp?' + query, '\n'.join(batch).encode(), 'text/plain; charset=utf-8')
        except urllib.error.HTTPError as exc:
            error = f'HTTP_{exc.code}'
            # Authentication/setup failures keep the batch for retry after repair.
            # Bad line/schema data must not poison the queue forever.
            permanent = exc.code in (400, 413, 422)
            exc.close()
        except (OSError, ValueError, TimeoutError): error = 'INFLUX_UNAVAILABLE'
        else:
            with self.lock:
                for _ in batch: self.pending.popleft()
                self.written += len(batch)
                self.last_write, self.last_error, self.status = time.time(), None, 'ok'
            return True
        with self.lock:
            if permanent:
                for _ in batch: self.pending.popleft()
                self.dropped += len(batch)
            changed = self.last_error != error
            self.last_error, self.status = error, 'error'
        if changed: LOG.warning('Telemetry archive: %s; inspect InfluxDB configuration/logs', error)
        return False

    def run(self, stop):
        if not self.enabled or not self.configured: return
        delay = 1
        while not stop.wait(delay):
            delay = 1 if self.flush_once() else min(delay * 2, 30)

    def history(self, node, minutes):
        if node not in NODES or type(minutes) is not int or not 1 <= minutes <= 1440:
            raise ValueError('invalid history range')
        if not self.enabled or not self.configured:
            raise ValueError('InfluxDB belum dikonfigurasi')
        # SELECT * works before every optional firmware field has appeared in the
        # schema. A single node and 2,000 most recent points bound each response.
        query = (f"SELECT * FROM telemetry WHERE node = '{node}' "
                 f"AND time >= now() - INTERVAL '{minutes} minutes' ORDER BY time DESC LIMIT 2000")
        body = self.request('/api/v3/query_sql', json.dumps(
            dict(db=self.database, q=query, format='json')).encode(), 'application/json')
        rows = json.loads(body)
        if not isinstance(rows, list) or len(rows) > 2000: raise ValueError('invalid history response')
        return dict(node=node, minutes=minutes, limit=2000, newest_first=True,
                    possibly_truncated=len(rows) == 2000, points=rows)
