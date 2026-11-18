# AETHER — implementasi lengkap tiga node

Kode aplikasi akhir tersedia untuk **Node B ESP32-S3**, **Node C ESP32 30-pin**
dan **Node A Raspberry Pi 5**. Sketch komponen 01–09 tetap tersedia untuk uji
wiring; gunakan firmware terintegrasi di bawah untuk menjalankan sistem lengkap.

| Bagian | Entry point |
| --- | --- |
| Firmware final Node B | [AetherNodeB.ino](firmware/node_b/AetherNodeB/AetherNodeB.ino) |
| Firmware final Node B tanpa microSD | [AetherNodeB_InternalFlash.ino](firmware/node_b/AetherNodeB_InternalFlash/AetherNodeB_InternalFlash.ino) |
| Firmware final Node C | [AetherNodeC.ino](firmware/node_c/AetherNodeC/AetherNodeC.ino) |
| Driver/protokol bersama | [AetherCore](firmware/libraries/AetherCore/src/) |
| Server API/daemon Node A | [app.py](server/aether/app.py), [runtime.py](server/aether/runtime.py) |
| Dashboard | [dashboard.html](server/aether/dashboard.html) |
| Instalasi Raspberry Pi | [install.sh](server/deploy/install.sh) |
| Riwayat implementasi | [PROGRES_IMPLEMENTASI.md](PROGRES_IMPLEMENTASI.md) |

Kontrak pin dan protokol tetap mengacu dokumen master. Dua keputusan pengguna:
**relay aktif HIGH** dan **payload radio 32 byte dengan reserved[7]**.

## Menyiapkan firmware

**Pilih satu firmware untuk Node B:** `AetherNodeB` menyimpan log di microSD
bawaan board; `AetherNodeB_InternalFlash` menyimpan log di flash internal melalui
LittleFS tanpa kartu SD. Keduanya memiliki fungsi sensor, WiFi, UART, radio dan
recovery yang sama. Buat `Secrets.h` di folder varian yang dipilih, lalu buka
file `.ino` dengan nama yang sama seperti foldernya. Upload salah satu varian.

1. Instal board package **esp32 by Espressif Systems 3.3.12**.
2. Instal library melalui Library Manager: **RF24 1.6.2**, **PubSubClient 2.8**,
   **ArduinoJson 7.4.2**, **Adafruit SSD1306 2.5.17**, **Adafruit GFX 1.12.6**
   dan dependensi **Adafruit BusIO 1.17.4**. Ini versi yang telah dikompilasi.
3. Salin `firmware/libraries/AetherCore` ke folder libraries Arduino, atau gunakan
   ZIP library dalam `dist/` melalui **Sketch → Include Library → Add .ZIP Library**.
4. Pada folder AetherNodeB dan AetherNodeC, salin `Secrets.h.example` menjadi
   **Secrets.h**, lalu isi SSID WiFi **2,4 GHz**, password, IP LAN Raspberry Pi,
   username dan password broker. File Secrets.h dikecualikan dari version control.
5. Buka entry point masing-masing board. Node B: **ESP32S3 Dev Module**, konektor
   UART, **USB CDC On Boot Disabled**. Node C: **DOIT ESP32 DEVKIT V1**.
   Pilih flash/PSRAM/partisi sesuai board fisik.
6. Format microSD 16 GB sebagai **FAT32** di komputer (allocation unit size default;
   label bebas, misalnya AETHER). Format menghapus isi kartu; cadangkan dahulu.
   Matikan Node B, masukkan kartu ke slot bawaan di belakang board, lalu nyalakan.
   Firmware final tidak memformat kartu otomatis; sketch 09 LittleFS tidak diperlukan.
7. Upload firmware final B/C. Serial Monitor **115200 baud**, perintah per baris.

Firmware final Node B menyimpan log ke **microSD**, memakai `SD_MMC` mode 1-bit
pada CLK **GPIO39**, CMD **GPIO38**, D0 **GPIO40**. Mapping ini mengikuti
[dokumentasi Freenove ESP32-S3 WROOM](https://docs.freenove.com/projects/fnk0086/en/latest/fnk0086/codes/tutorial/4_Read_and_Write_the_SDcard.html),
yang bentuk board-nya menyerupai foto pengguna; pembacaan kartu pada board aktual
masih perlu diuji. Pin dapat diubah lewat `AETHER_SD_CLK`, `AETHER_SD_CMD`,
`AETHER_SD_D0` di Secrets.h jika varian board berbeda. Jangan gunakan GPIO38–40
untuk peripheral lain. SD_MMC disediakan board package ESP32, tanpa library tambahan.

Serial Monitor menampilkan `[LOG] microSD ready` setelah file BOOT berhasil ditulis.
File `/aether-current.jsonl` dan `/aether-previous.jsonl` berada di root kartu,
dengan rotasi maksimal **32 KiB per file**, mengikuti batas blackbox sebelumnya.
Isinya log event/insiden, bukan arsip seluruh telemetri. Data lama LittleFS tidak
dipindahkan otomatis dan firmware final tidak lagi membutuhkan partisi LittleFS.
Jika kartu gagal di-mount/ditulis, fungsi lain tetap berjalan, error dicetak ke
Serial, dan log yang gagal tidak tersimpan atau dialihkan ke flash internal.
Setelah memperbaiki kartu, restart Node B. Matikan board sebelum mencabut kartu.
Sketch komponen 09 tetap merupakan uji LittleFS internal yang terpisah.

Untuk varian **AetherNodeB_InternalFlash**, pilih partition scheme dengan partisi
filesystem `spiffs` minimal **128 KiB**. Siapkan LittleFS menggunakan sketch
`09_littlefs_blackbox` dengan board dan layout partisi yang sama; perintah serial
**FORMAT** menghapus seluruh isi partisi filesystem internal, bukan kartu SD.
Setelah itu upload `AetherNodeB_InternalFlash` tanpa opsi erase seluruh flash.
Saat berhasil muncul `[LOG] LittleFS ready`; firmware ini tidak memformat otomatis.
File current/previous dan batas rotasi 32 KiB per file sama dengan versi microSD,
namun berada di flash internal dan tidak terlihat sebagai drive USB.

Kalibrasi ADC dari bagian 02 dimasukkan ke `AETHER_VOLTAGE_GAIN` dan
`AETHER_VOLTAGE_OFFSET_V` di Secrets.h Node B.

Kompilasi CLI dari akar proyek, dengan library eksternal sudah tersedia:

```powershell
./tools/build_firmware.ps1
```

Script mendukung `-ArduinoCli`, `-ConfigFile`, dan `-ExtraLibraries`. Arduino CLI
memakai `--library firmware/libraries/AetherCore`; tidak memerlukan instalasi
library AetherCore global jika memakai script ini.

## Menyiapkan Raspberry Pi 5

Penyimpanan backend memakai **InfluxDB 3 Core untuk riwayat telemetri** dan
**SQLite untuk insiden/recovery**. Setup database, token, retensi dan contoh
endpoint tersedia di [panduan InfluxDB + SQLite](docs/INFLUXDB_SQLITE.md).
Firmware ESP32 tetap mengirim MQTT/CoAP; token InfluxDB hanya di `server/.env`.
Konfigurasi Docker tersedia di `server/deploy/compose.influx.yml`.

1. Tempatkan repositori pada **`/opt/aether`**, lalu salin
   `server/.env.example` menjadi `server/.env` dan isi password/API key.
   IP Node B boleh dikosongkan agar ditemukan dari telemetri; reservation DHCP
   atau IP tetap disarankan agar alamat server konsisten.
2. Aktifkan UART header sesuai master dengan `dtparam=uart0=on` di
   `/boot/firmware/config.txt`, lalu reboot. Nonaktifkan serial login pada port
   yang dipakai. Periksa `/dev/ttyAMA*` dan mapping header, jangan menganggap
   `/dev/serial0` pasti menunjuk header GPIO pada Pi 5.
3. Default `AETHER_SERIAL_PORT=/dev/ttyAMA0`, 9600 **8N1**; sesuaikan jika mapping
   OS aktual berbeda. TX Pi pin 8 → RX18 B, RX Pi pin 10 ← TX17 B, GND bersama.
4. Jalankan persiapan berikut di Pi:

```bash
cd /opt/aether
sudo bash server/deploy/install.sh
sudo mosquitto_passwd -c /etc/mosquitto/aether-passwords aether
sudo chown root:mosquitto /etc/mosquitto/aether-passwords
sudo chmod 0640 /etc/mosquitto/aether-passwords
sudo install -m 0644 server/deploy/aether-mosquitto.conf /etc/mosquitto/conf.d/aether.conf
sudo systemctl restart mosquitto
sudo systemctl start aether-target aether-api
```

Gunakan password broker yang sama di `.env` dan kedua Secrets.h. Jangan memakai
`mosquitto_passwd -c` pada file kredensial lama yang perlu dipertahankan; tanpa
`-c` menambah/mengubah user pada file yang ada. Config listener ini ditujukan
untuk LAN proyek. Periksa listener broker lain sebelum menambahkan port 1883.

Installer menyiapkan venv dengan system site packages untuk gpiozero/lgpio,
user aether, izin data, unit systemd dan sudoers **khusus restart
aether-target.service**. Installer tidak otomatis mengubah boot config atau
memulai API sebelum konfigurasi diverifikasi. Jangan menjalankan API dengan
lebih dari satu worker: UART/GPIO/collector dimiliki satu runtime.

Buka **`http://<IP-Pi>:8000`**. API key di dashboard berlaku untuk operasi POST;
tidak disimpan browser. Cek layanan dengan:

```bash
systemctl status aether-target aether-api mosquitto
journalctl -u aether-api -f
```

## Alur sistem

- B membaca GY-906 I2C 100 kHz dan ADC, Modbus lima register, serta HTTP probe B.
  Telemetri MQTT tiap dua detik dan radio tiap satu detik menggunakan data aktual.
- C menerima radio, menampilkan OLED/LED, menguji HTTP probe C dan menerbitkan
  telemetri MQTT. Tombol **GPIO33** meminta recovery lewat radio; CLI `r` recovery,
  `m` mute, `i` status. ACK radio belum berarti recovery selesai.
- B menangani radio/MQTT/CoAP command melalui relay aktif HIGH atau mute buzzer.
  Tombol BOOT GPIO0 memicu mute. CLI B `m`, `u`, `r`, `i` diikuti newline.
- Task HTTP/MQTT/CoAP dan task logger terpisah dari loop aktuator/radio. Flash IO
  dan peripheral masih dapat menimbulkan latensi; tidak ada klaim hard real-time.
- Node A menyediakan broker config, API/dashboard, SQLite insiden, Modbus slave,
  GPIO17 recovery input, target service terpisah dan collector CoAP port 5683.

## Diagnosis dan recovery otomatis

Ambang IR default **70 °C**, ADC drop **<4,5 V**. Ambang IR adalah pilihan
implementasi yang dapat diubah di Secrets.h; kalibrasi sensor tetap diperlukan.
Data hilang tidak diperlakukan sebagai sehat. Kombinasi host tidak merespons
ditandai **FULL_HOST_DOWN_SUSPECTED**, bukan bukti pasti kernel mati.
Probe C kedaluwarsa dilaporkan EXTERNAL_PROBE_STALE, link RF hilang sebagai
RADIO_LINK_UNAVAILABLE. Dashboard menampilkan DATA_STALE saat API terputus.

Recovery otomatis hanya untuk **SERVICE_DOWN dengan host Modbus masih hidup**:
grace boot 30 detik, fault menetap minimal 10 detik, FC06 restart terlebih dahulu.
Jika belum pulih, relay dicoba setelah **35 detik**, melewati throttle 30 detik
daemon Pi. Setelah itu jeda 60 detik untuk siklus berikutnya, maksimal tiga siklus
sampai HEALTHY kembali. Thermal/power/sensor/network fault tidak memicu reboot
otomatis. Recovery manual tetap melewati cooldown relay 30 detik.

Relay mengaktifkan GPIO17 Pi melalui kontak NO/COM; daemon merestart target
service dan menghapus fault simulasi. **Ini bukan sambungan pin reset hardware**;
kernel/host yang macet total memerlukan mekanisme reset/power terpisah yang tidak
ada pada wiring proyek. Kode tidak mengklaim kemampuan tersebut.

## Demo dan protokol

Dashboard mendukung NORMAL, SERVICE_CRASH, EXT_BLOCK dan OVERHEAT_SIM. Simulasi
service crash membuat probe 503 tanpa mematikan dashboard. EXT_BLOCK memberi
B=200, C=403. OVERHEAT_SIM dikirim lewat MQTT dan kedaluwarsa di B jika tidak
diperbarui 30 detik; nilai IR fisik tetap dilaporkan terpisah dari efek diagnosis.

REST: `/api/v1/status`, `/api/v1/incidents`, `/api/v1/fault/inject`,
`/api/v1/actuator/cmd`. Command broker dilaporkan queued, bukan executed.
GPIO/Modbus daemon mengakui permintaan restart, bukan keberhasilan service pulih.

CoAP B: GET `/3/0`, GET `/3303/0/5700`, PUT `/3311/0/5850` (`1` recovery,
`0` mute). PUT duplikat dideteksi dengan MID/peer/hash dan cache TTL 247 detik.
Collector A menerima POST `/aether/nodeB`, serta polling dua resource B.
**LwM2M/IPSO di sini adalah subset resource URI sesuai kontrak**, bukan stack
bootstrap/registration/DTLS/Observe LwM2M lengkap. UART 8N1 dan penerimaan
RTU polling mengikuti proyek; tidak mengklaim kepatuhan timing t1.5 penuh.

## Menjalankan tanpa perangkat

Windows PowerShell:

```powershell
cd server
python -m venv .venv
./.venv/Scripts/python.exe -m pip install -r requirements-dev.txt
./.venv/Scripts/python.exe -m aether.demo
```

Linux/macOS:

```bash
cd server
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python -m aether.demo
```

Buka localhost:8000. Badge **SIMULASI** menandai data sintetis; mode ini tidak
membuka GPIO/serial/MQTT dan tidak menggerakkan aktuator. Hentikan dengan Ctrl+C.
Server aktual dijalankan dengan `uvicorn aether.app:create_app --factory --host
0.0.0.0 --port 8000`, setelah environment diisi; `.env` dimuat systemd, bukan
otomatis oleh Python/uvicorn.

## Pengujian dan batas hasil

```powershell
./tools/test_firmware.ps1
```

Menjalankan 11 executable tes host terhadap **library final bersama**, ditambah
parser JSON independen. Tes server: `cd server` lalu `python -m pytest tests -q`.
Tes browser opsional terdapat di `tools/verify_dashboard.mjs` dan memakai server
demo serta Playwright Core yang dipasang di `.build/browser`.

Firmware B/C telah dikompilasi; server/API, collector UDP lokal, codec lintas
bahasa, kebijakan recovery dan dashboard desktop/mobile telah diuji. **Belum ada
upload, uji wiring, radio fisik, kalibrasi, GPIO Raspberry Pi atau stress/power-loss
test pada perangkat**. Isi konfigurasi dan jalankan pengujian hardware sebelum
menganggap pemulihan server terverifikasi. Lihat [VALIDASI.md](VALIDASI.md).

Referensi teknis: [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/),
[RF24](https://rf24.readthedocs.io/en/stable/classRF24.html),
[konfigurasi UART Raspberry Pi](https://www.raspberrypi.com/documentation/computers/configuration.html),
[RFC 7252](https://www.rfc-editor.org/rfc/rfc7252.html).
