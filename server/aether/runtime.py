from __future__ import annotations

import hashlib
import json
import logging
import os
import queue
import socket
import subprocess
import threading
import time
import urllib.request
from pathlib import Path

import paho.mqtt.client as mqtt
import serial

from . import coap, rtu
from .state import State, clean
from .influx import InfluxTelemetry

LOG = logging.getLogger('aether')


class Runtime:
    def __init__(self, state: State):
        self.state = state
        self.influx = InfluxTelemetry.from_env()
        self.archive_lock = threading.Lock()
        self.last_archive = {}
        self.stop = threading.Event()
        self.jobs = queue.Queue(maxsize=8)
        self.job_lock = threading.Lock()
        self.last_recovery = -1000.0
        self.threads = []
        self.button = None
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='aether-nodeA')
        self.client.will_set('aether/nodeA/status', 'offline', retain=True)
        user = os.getenv('AETHER_MQTT_USER', '')
        if user: self.client.username_pw_set(user, os.getenv('AETHER_MQTT_PASSWORD', ''))
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.reconnect_delay_set(1, 30)

    def on_connect(self, client, userdata, flags, reason, properties):
        connected = reason == 0
        with self.state.lock: self.state.links['mqtt'] = connected
        if connected:
            for topic in ('aether/nodeB/telemetry', 'aether/nodeC/telemetry', 'aether/alert/incident', 'aether/nodeB/status'):
                client.subscribe(topic, qos=1)
            client.publish('aether/nodeA/status', 'online', retain=True)
            self.publish_mode()

    def on_disconnect(self, client, userdata, flags, reason, properties):
        with self.state.lock: self.state.links['mqtt'] = False

    def on_message(self, client, userdata, message):
        try:
            if len(message.payload) > 4096: return
            if message.topic == 'aether/nodeB/status':
                if message.payload == b'online': self.publish_mode()
                return
            data = json.loads(message.payload, parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
            if not isinstance(data, dict): return
            if message.topic.endswith('/telemetry'):
                self.ingest_telemetry(message.topic.split('/')[1], data)
            elif message.topic == 'aether/alert/incident':
                self.state.incident(str(data.get('source', 'unknown')), str(data.get('diagnosis', 'UNKNOWN')),
                                    str(data.get('evidence', '')), str(data.get('action', 'NONE')))
        except (ValueError, TypeError, UnicodeError): LOG.warning('Rejected MQTT payload on %s', message.topic)

    def ingest_telemetry(self, node, data):
        self.state.ingest(node, data)
        # MQTT and CoAP can report the same sample. Archive at most once/second
        # per node, allowing CoAP to continue archiving when MQTT is unavailable.
        with self.archive_lock:
            now = time.monotonic()
            if now - self.last_archive.get(node, -1000) >= 1:
                self.influx.enqueue(node, data)
                self.last_archive[node] = now

    def storage_status(self):
        return {'incidents': {'engine': 'sqlite', 'retention_count': 1000},
                'telemetry': self.influx.snapshot()}

    def telemetry_history(self, node, minutes):
        return self.influx.history(node, minutes)

    def publish(self, topic, data):
        if not self.client.is_connected(): return False
        result = self.client.publish(topic, json.dumps(clean(data), allow_nan=False), qos=1, retain=False)
        return result.rc == mqtt.MQTT_ERR_SUCCESS

    def publish_mode(self):
        with self.state.lock: mode = self.state.mode
        return self.publish('aether/demo/fault', {'mode': mode})

    def actuator(self, target):
        if target == 'COAP_POLL': return self.poll_coap()
        payload = {'action': 'TRIGGER_RECOVERY' if target == 'RELAY_RECOVERY' else 'MUTE_ALARM',
                   'mute_buzzer': target == 'MUTE_ALARM', 'request_id': os.urandom(8).hex()}
        accepted = self.publish('aether/cmd/actuator', payload)
        return {'queued_to_broker': accepted, 'executed': False, 'command': payload}

    def recovery(self, source):
        with self.job_lock:
            now = time.monotonic()
            if now - self.last_recovery < 30: return False
            try: self.jobs.put_nowait(source)
            except queue.Full: return False
            self.last_recovery = now
            return True

    def worker(self):
        while not self.stop.is_set():
            try: source = self.jobs.get(timeout=.2)
            except queue.Empty: continue
            try:
                if os.getenv('AETHER_ALLOW_SYSTEMCTL', '0') == '1':
                    subprocess.run(['sudo', '-n', '/usr/bin/systemctl', 'restart', 'aether-target.service'],
                                   check=True, timeout=20, capture_output=True)
                with self.state.lock: self.state.mode = 'NORMAL'
                self.publish_mode()
                self.state.incident('nodeA', 'RECOVERY_REQUEST', source, 'TARGET_RESTARTED' if os.getenv('AETHER_ALLOW_SYSTEMCTL') == '1' else 'DEMO_FAULT_CLEARED')
            except (subprocess.SubprocessError, OSError) as exc:
                self.state.incident('nodeA', 'RECOVERY_FAILED', type(exc).__name__, 'CHECK_TARGET_SERVICE')

    def registers(self):
        with self.state.lock:
            host = self.state.host.copy()
            running = host['service_running'] and self.state.mode != 'SERVICE_CRASH'
        if host['cpu_temp_c'] is None: return None
        return [max(0, min(65535, round(host['cpu_temp_c'] * 10))), host['ram_pct'], host['load_pct'],
                int(running), int(host['uptime_s']) & 0xffff]

    def uart_worker(self):
        port_name = os.getenv('AETHER_SERIAL_PORT', '/dev/ttyAMA0')
        while not self.stop.is_set():
            try:
                with serial.Serial(port_name, 9600, bytesize=8, parity='N', stopbits=1,
                                   timeout=.01, write_timeout=.2) as port:
                    with self.state.lock: self.state.links['uart'] = True
                    buffer = bytearray()
                    while not self.stop.is_set():
                        chunk = port.read(256)
                        if chunk:
                            buffer.extend(chunk)
                            if len(buffer) > 256: buffer.clear()
                        elif buffer:
                            packet = bytes(buffer); buffer.clear()
                            reply = rtu.response(packet, self.registers, self.recovery)
                            if reply is not None:
                                with self.state.lock: self.state.modbus_seen = time.monotonic()
                                port.write(reply); port.flush()
            except (serial.SerialException, OSError):
                with self.state.lock: self.state.links['uart'] = False
                self.stop.wait(3)

    def host_worker(self):
        archived_at = -1000.0
        while not self.stop.is_set():
            host = {'cpu_temp_c': None, 'ram_pct': 0, 'load_pct': 0, 'uptime_s': int(time.monotonic()), 'service_running': False}
            try:
                host['cpu_temp_c'] = int(Path('/sys/class/thermal/thermal_zone0/temp').read_text()) / 1000
                values = {line.split(':')[0]: int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines()}
                host['ram_pct'] = max(0, min(100, round(100 * (1 - values['MemAvailable'] / values['MemTotal']))))
                host['load_pct'] = max(0, min(100, round(os.getloadavg()[0] / (os.cpu_count() or 1) * 100)))
                host['uptime_s'] = int(float(Path('/proc/uptime').read_text().split()[0]))
            except (OSError, ValueError, KeyError): pass
            if os.getenv('AETHER_SIMULATE_HARDWARE') == '1':
                host.update(cpu_temp_c=49.2, ram_pct=25, load_pct=10, simulated=True)
            try:
                with urllib.request.urlopen(os.getenv('AETHER_TARGET_URL', 'http://127.0.0.1:8001/health'), timeout=.5) as result:
                    host['service_running'] = result.status == 200
            except (OSError, ValueError): pass
            with self.state.lock: self.state.host = host
            now = time.monotonic()
            if now - archived_at >= 5:
                self.influx.enqueue('nodeA', dict(cpu_temp_c=host['cpu_temp_c'],
                    cpu_ram_pct=host['ram_pct'], cpu_load_pct=host['load_pct'],
                    service_running=host['service_running'], uptime_ms=host['uptime_s'] * 1000,
                    simulated=host.get('simulated', False)))
                archived_at = now
            self.stop.wait(1)

    def node_b_host(self):
        configured = os.getenv('AETHER_NODE_B_HOST', '')
        with self.state.lock: discovered = self.state.nodes.get('nodeB', {}).get('ip', '')
        import ipaddress
        candidate = configured or discovered
        if not candidate: raise ValueError('Node B IP belum dikonfigurasi/ditemukan')
        address = ipaddress.ip_address(candidate)
        if address.version != 4 or not address.is_private: raise ValueError('Node B harus IP LAN IPv4')
        return str(address)

    def poll_coap(self):
        host = self.node_b_host()
        results = {path: coap.request(host, path) for path in ('/3/0', '/3303/0/5700')}
        if all(item['code'] == '2.05' for item in results.values()):
            with self.state.lock: self.state.coap_seen = time.monotonic()
        return results

    def coap_worker(self):
        cache = {}
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind((os.getenv('AETHER_BIND', '0.0.0.0'), 5683)); sock.settimeout(.5)
        except OSError:
            LOG.exception('CoAP collector unavailable'); return
        with sock:
            while not self.stop.is_set():
                try: raw, address = sock.recvfrom(2049)
                except socket.timeout: continue
                except OSError:
                    if self.stop.is_set(): break
                    continue
                try:
                    message = coap.decode(raw)
                    if message.kind not in (0, 1) or message.code not in (1, 2): continue
                    key = (address, message.mid)
                    now = time.monotonic()
                    cache = {k: v for k, v in cache.items() if now - v[0] < 247}
                    digest = hashlib.sha256(raw).digest()
                    if key in cache:
                        if cache[key][1] == digest: sock.sendto(cache[key][2], address)
                        continue
                    code, payload = 132, b'not found'
                    unsupported = any(number & 1 and number not in (3, 7, 11, 15, 17) for number, _ in message.options)
                    if unsupported:
                        code, payload = 130, b'bad option'
                    elif message.code == 1 and message.path == '/3/0':
                        code, payload = 69, b'{"mfg":"AETHER","model":"Pi5-Collector"}'
                    elif message.code == 2 and message.path == '/aether/nodeB':
                        data = json.loads(message.payload)
                        self.ingest_telemetry('nodeB', data)
                        with self.state.lock: self.state.coap_seen = now
                        code, payload = 68, b''
                    reply = coap.encode(coap.Message(2 if message.kind == 0 else 1, code, message.mid,
                                                    message.token, ((12, b'\x32'),) if payload.startswith(b'{') else (), payload))
                    if len(cache) >= 128: cache.pop(next(iter(cache)))
                    cache[key] = (now, digest, reply)
                    sock.sendto(reply, address)
                except (ValueError, UnicodeError, KeyError, TypeError, OSError): continue

    def collector_worker(self):
        while not self.stop.wait(10):
            self.publish_mode()
            try: self.poll_coap()
            except (OSError, ValueError): pass

    def start(self):
        thread = threading.Thread(target=self.influx.run, args=(self.stop,), daemon=True)
        thread.start(); self.threads.append(thread)
        self.client.connect_async(os.getenv('AETHER_MQTT_HOST', '127.0.0.1'),
                                  int(os.getenv('AETHER_MQTT_PORT', '1883')), keepalive=20)
        self.client.loop_start()
        for target in (self.worker, self.host_worker, self.uart_worker, self.coap_worker, self.collector_worker):
            thread = threading.Thread(target=target, daemon=True); thread.start(); self.threads.append(thread)
        if os.getenv('AETHER_GPIO', '0') == '1':
            try:
                from gpiozero import Button
                self.button = Button(17, pull_up=True, bounce_time=.1)
                self.button.when_pressed = lambda: self.recovery('GPIO17_RELAY')
                with self.state.lock: self.state.links['gpio'] = True
            except Exception: LOG.exception('GPIO recovery unavailable; API remains active')

    def close(self):
        self.stop.set()
        if self.button: self.button.close()
        self.client.disconnect(); self.client.loop_stop()
        for thread in self.threads: thread.join(timeout=2)
