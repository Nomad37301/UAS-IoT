"""Explicit local simulation: no GPIO, serial, MQTT or actuator access."""
import threading
import time

import uvicorn

from .app import create_app
from .state import State

state = State()
state.host.update(cpu_temp_c=49.2, ram_pct=34, load_pct=20, service_running=True, simulated=True)


class DemoRuntime:
    def publish_mode(self): return True
    def actuator(self, target):
        if target == 'COAP_POLL': return {'simulated': True, 'resources': ['/3/0', '/3303/0/5700']}
        if target == 'RELAY_RECOVERY':
            with state.lock: state.mode = 'NORMAL'
        state.incident('demo', 'SIMULATED_COMMAND', target, 'NO_PHYSICAL_ACTUATION')
        return {'queued_to_broker': True, 'executed': False, 'simulated': True}


runtime = DemoRuntime()
app = create_app(state, runtime, start_workers=False)


def feed():
    seq = 0
    while True:
        with state.lock: mode = state.mode
        b_code = 503 if mode == 'SERVICE_CRASH' else 200
        c_code = 403 if mode == 'EXT_BLOCK' else b_code
        diagnosis = {'SERVICE_CRASH': 'SERVICE_DOWN', 'OVERHEAT_SIM': 'HW_ANOMALY'}.get(mode, 'HEALTHY')
        state.ingest('nodeB', dict(system='AETHER', node='nodeB', state=diagnosis, simulated=True,
                                  http_code=b_code, voltage_v=5.04, ir_obj_c=80 if mode == 'OVERHEAT_SIM' else 37.8,
                                  ir_amb_c=27.1, modbus_ok=True, sensor_ok=True))
        state.ingest('nodeC', dict(system='AETHER', node='nodeC', http_ext_code=c_code, simulated=True,
                                  rf_rx_alive=True, last_rf_seq=seq))
        seq = (seq + 1) & 0xffff
        time.sleep(.2)


if __name__ == '__main__':
    threading.Thread(target=feed, daemon=True).start()
    uvicorn.run(app, host='127.0.0.1', port=8000)
