# Validasi implementasi final AETHER

Tanggal pengerjaan: 18 November 2026. Semua hasil di bawah adalah hasil nyata
toolchain/test lokal; simulasi tidak dihitung sebagai pengujian perangkat fisik.

| Area | Hasil | Bukti |
| --- | --- | --- |
| Driver final bersama | PASS | 11 executable C++ host dari tools/test_firmware.ps1, -Wall -Wextra -Werror |
| Diagnosis/recovery | PASS | Kombinasi sensor/probe, grace, sustained fault, tiga siklus termasuk fallback ketiga, rollover |
| Paket radio | PASS | Packed 32 byte, byte golden, offset, reserved[7], validasi command dan seq |
| CoAP lintas bahasa | PASS | Datagram report C++ didekode parser Python independen |
| Parser CoAP | PASS | Frame malformed dan 10.000 input fuzz pada parser C++ |
| Server/API | PASS | 27 tes pytest; fault modes, auth, antrean broker gagal, freshness, SQLite, Modbus, MQTT, adapter InfluxDB |
| Adapter InfluxDB 3 | PASS | Line protocol, tipe field, antrean terbatas, retry timestamp identik, error HTTP, worker HTTP localhost, riwayat API/auth |
| Collector UDP | PASS | Socket localhost, POST telemetry, duplikat MID, bad payload dan critical option |
| Target service | PASS | HTTP server terpisah nyata di localhost; worker mendeteksi target hidup lalu mati |
| Dashboard | PASS | Chrome headless, desktop 1440 px dan mobile 390 px, tidak ada JS page error |
| Alur dashboard | PASS | EXT_BLOCK → C=403, SERVICE_CRASH, recovery simulasi → HEALTHY, API offline → DATA_STALE |
| Firmware Node B | PASS | Arduino ESP32-S3, core 3.3.12, full sketch terintegrasi |
| Firmware Node B tanpa microSD | PASS | AetherNodeB_InternalFlash, LittleFS, ESP32-S3 core 3.3.12; kompilasi 19 November 2026 |
| Firmware Node C | PASS | Arduino ESP32 DOIT DevKit V1, core 3.3.12, full sketch terintegrasi |

Node C hasil build: **1.003.524 byte flash (76%)**, **52.544 byte RAM global (16%)**.
Node B hasil build final setelah perubahan microSD: **1.060.929 byte flash (80%)**,
**51.032 byte RAM global (15%)**, FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
RAM global bukan pengukuran peak heap/task stack.

Varian Node B tanpa microSD: **1.037.945 byte flash (79%)**, **50.848 byte RAM
global (15%)**, FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`. Perbandingan sumber
memastikan logika selain penyimpanan identik dengan varian microSD. Tes logger
host dan parser JSON independen dijalankan ulang: PASS. Persistensi LittleFS pada
board aktual belum diuji.

Perubahan microSD: firmware final Node B dikompilasi ulang dengan SD_MMC bawaan
core 3.3.12, mode 1-bit, CLK39/CMD38/D0=40, mount tanpa auto-format. Executable tes
logger bersama dan parser JSON independen dijalankan ulang: PASS. Tes tersebut
memeriksa perilaku logger melalui filesystem tiruan, bukan driver/kartu SD fisik.

Library teruji: RF24 1.6.2, PubSubClient 2.8, ArduinoJson 7.4.2,
Adafruit SSD1306 2.5.17, GFX 1.12.6 dan BusIO 1.17.4.
Python pengujian: 3.14; runtime memakai API yang kompatibel Python 3.11+.
Ada satu deprecation warning dependency TestClient/httpx; tes tetap lulus.

Integrasi InfluxDB + SQLite ditambahkan 19 November 2026. Seluruh 27 tes server
lulus. Tes InfluxDB menggunakan transport tiruan dan server HTTP localhost dengan
kontrak API v3; **bukan engine InfluxDB nyata**. File Compose lolos
`docker compose -f server/deploy/compose.influx.yml config --quiet`. Docker daemon
lokal tidak aktif, sehingga container, autentikasi/retensi engine, SQL aktual,
volume persisten dan pemakaian RAM/disk pada Pi belum diverifikasi. Panduan setup
tersedia di `docs/INFLUXDB_SQLITE.md`; tidak ada perubahan firmware ESP32.

## Yang belum terverifikasi

- Belum upload ke ESP32 atau Raspberry Pi dan belum menjalankan installer di Pi.
- Belum menjalankan engine InfluxDB 3 Core atau memverifikasi write/query/retensi
  pada engine tersebut; pengujian lokal memverifikasi adapter dan API melalui HTTP.
- Belum menguji mapping pin slot microSD pada varian board pengguna, mount FAT32,
  penulisan/pembacaan kartu fisik, kartu penuh, pencabutan kartu atau power loss SD.
- Belum menguji GPIO17/lgpio, serial header Pi, sensor/ADC, radio fisik,
  OLED/LED/buzzer, relay mekanis, atau jaringan WiFi/broker perangkat nyata.
- Belum menguji jangkauan radio, peak memory, stress multi-hari, EMI dan power loss.
- Broker Mosquitto nyata dan komunikasi firmware di perangkat belum diuji end-to-end;
  parsing MQTT/backend serta kompilasi client telah diuji.
- Belum mengukur timing RTU per-byte/t1.5 di wire; konfigurasi 8N1 mengikuti proyek.
- CoAP/LwM2M adalah resource-model subset sesuai kontrak, bukan stack LwM2M lengkap.
- Recovery GPIO membutuhkan daemon/kernel Pi berjalan; wiring ini tidak menyediakan
  reset/power-cycle independen saat host macet total.

## Mengulangi pengujian

```powershell
./tools/test_firmware.ps1
./tools/build_firmware.ps1
```

```bash
cd server
pip install -r requirements-dev.txt
python -m pytest tests -q
python -m aether.demo
```

Untuk tes browser, pasang `playwright-core` di `.build/browser`, jalankan demo,
lalu `node tools/verify_dashboard.mjs`. Script saat ini memakai executable Chrome
Windows; sesuaikan path bila dijalankan di OS lain. Screenshot menggunakan data
sintetis yang ditandai SIMULASI, bukan pembacaan perangkat.

Lihat [dashboard desktop](docs/dashboard-desktop.png) dan
[dashboard mobile](docs/dashboard-mobile.png).
