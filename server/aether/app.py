from __future__ import annotations

import asyncio
import os
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Literal

from fastapi import Depends, FastAPI, Header, HTTPException, Query
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel, ConfigDict

from .runtime import Runtime
from .state import State


class Fault(BaseModel):
    model_config = ConfigDict(extra='forbid')
    mode: Literal['NORMAL', 'SERVICE_CRASH', 'EXT_BLOCK', 'OVERHEAT_SIM']


class Actuator(BaseModel):
    model_config = ConfigDict(extra='forbid')
    target: Literal['RELAY_RECOVERY', 'MUTE_ALARM', 'COAP_POLL']


def create_app(state=None, runtime=None, start_workers=True):
    state = state or State(os.getenv('AETHER_DATABASE', 'data/aether.sqlite'))
    runtime = runtime or Runtime(state)
    @asynccontextmanager
    async def lifespan(app):
        if start_workers: runtime.start()
        yield
        if start_workers: runtime.close()

    app = FastAPI(title='AETHER Node A', lifespan=lifespan)
    app.state.aether, app.state.runtime = state, runtime
    def authorize(authorization: str | None = Header(default=None)):
        key = os.getenv('AETHER_API_KEY', '')
        import secrets
        if key and not secrets.compare_digest(authorization or '', 'Bearer ' + key):
            raise HTTPException(401, 'Bearer token required')

    @app.get('/')
    def dashboard():
        return FileResponse(Path(__file__).with_name('dashboard.html'))

    @app.get('/health')
    def health(probe: Literal['B', 'C'] = 'B'):
        with state.lock:
            failed = state.mode == 'SERVICE_CRASH' or not state.host['service_running']
            blocked = state.mode == 'EXT_BLOCK' and probe == 'C'
        code = 503 if failed else 403 if blocked else 200
        return JSONResponse({'system': 'AETHER', 'status': 'ok' if code == 200 else 'unavailable'}, status_code=code)

    @app.get('/api/v1/status')
    def status(): return state.snapshot()

    @app.get('/api/v1/incidents')
    def incidents(limit: int = Query(100, ge=1, le=1000)): return state.incidents(limit)

    @app.get('/api/v1/storage')
    def storage():
        if not hasattr(runtime, 'storage_status'):
            return {'incidents': {'engine': 'sqlite', 'retention_count': 1000},
                    'telemetry': {'engine': 'influxdb3', 'enabled': False, 'status': 'disabled'}}
        return runtime.storage_status()

    @app.get('/api/v1/telemetry/history', dependencies=[Depends(authorize)])
    async def history(node: Literal['nodeA', 'nodeB', 'nodeC'] = 'nodeB',
                      minutes: int = Query(60, ge=1, le=1440)):
        if not hasattr(runtime, 'telemetry_history'):
            raise HTTPException(503, 'Riwayat InfluxDB tidak tersedia pada mode ini')
        try: return await asyncio.to_thread(runtime.telemetry_history, node, minutes)
        except (OSError, ValueError, TimeoutError):
            raise HTTPException(503, 'Riwayat belum tersedia; periksa konfigurasi, data, dan layanan InfluxDB')

    @app.post('/api/v1/fault/inject', dependencies=[Depends(authorize)])
    def inject(body: Fault):
        with state.lock: state.mode = body.mode
        delivered = runtime.publish_mode()
        state.incident('dashboard', body.mode, 'Explicit fault simulation', 'FAULT_INJECTED')
        return {'mode': body.mode, 'demo_message_queued': delivered}

    @app.post('/api/v1/actuator/cmd', dependencies=[Depends(authorize)])
    async def actuator(body: Actuator):
        try: result = await asyncio.to_thread(runtime.actuator, body.target)
        except (OSError, ValueError) as exc: raise HTTPException(503, str(exc)) from exc
        if body.target != 'COAP_POLL' and not result['queued_to_broker']:
            raise HTTPException(503, 'MQTT broker unavailable; command was not queued')
        return result

    return app
