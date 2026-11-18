# InfluxDB + SQLite pada Raspberry Pi

Implementasi memakai **InfluxDB 3 Core**, API v3 (bukan API/Flux InfluxDB 2).
SQLite tetap menjadi penyimpanan riwayat insiden dan recovery, maksimal 1.000
entri terbaru. File SQLite lama tetap dipakai; tidak perlu konversi atau dihapus.

```text
ESP32 B/C -- MQTT atau CoAP --> backend Raspberry Pi
                                  |-- telemetri --> antrean --> InfluxDB 3
                                  |-- insiden/recovery ------> SQLite
                                  `-- status terkini -------> dashboard
```

Firmware kedua varian Node B dan Node C tidak perlu diubah. Kredensial InfluxDB
hanya ada di `server/.env` pada Raspberry Pi, bukan di `Secrets.h` ESP32.

## 1. Jalankan InfluxDB

Gunakan Raspberry Pi OS **64-bit** dan Docker Engine dengan Compose plugin yang
sudah terpasang. Panduan resmi Docker: https://docs.docker.com/engine/install/debian/.
Compose ini memakai image `influxdb:3.12.2-core`, tersedia untuk ARM64. Pilihan
versi merujuk dokumentasi resmi; image Docker/engine database belum dijalankan di
komputer pengembangan karena Docker daemon lokal tidak aktif.

Dari folder proyek `/opt/aether` pada Pi:

```bash
sudo docker compose -f server/deploy/compose.influx.yml up -d
sudo docker compose -f server/deploy/compose.influx.yml logs --tail=30 influxdb
```

Layanan hanya dipublikasikan pada **127.0.0.1:8181**. Backend berjalan pada host
Pi dan dapat mengaksesnya. Data persisten berada pada Docker volume
`deploy_influx-data` (nama dapat berbeda jika project name Compose diubah).
Jangan menjalankan `docker compose down -v` jika data ingin dipertahankan.

## 2. Buat token dan database, satu kali

Pada instalasi baru yang belum memiliki token:

```bash
sudo docker compose -f server/deploy/compose.influx.yml exec influxdb influxdb3 create token --admin
```

Simpan token yang ditampilkan. Jika instance sudah mempunyai token, gunakan token
admin yang ada; jangan menganggap perintah bootstrap dapat dijalankan ulang tanpa
autentikasi. Token admin memiliki akses luas, sehingga simpan hanya pada Pi dan
pertahankan binding port localhost.

Buat database dengan retensi **30 hari**. Perintah berikut membaca token tanpa
menuliskannya secara literal di riwayat shell:

```bash
read -rsp 'Token admin InfluxDB: ' AETHER_SETUP_TOKEN
echo
sudo docker compose -f server/deploy/compose.influx.yml exec -T \
  -e INFLUXDB3_AUTH_TOKEN="$AETHER_SETUP_TOKEN" influxdb \
  influxdb3 create database --retention-period 30d aether
unset AETHER_SETUP_TOKEN
```

Jika database `aether` sudah ada, jangan menghapusnya untuk mengulang setup.
Pastikan retensi database yang ada sesuai kebutuhan. Retensi membatasi umur data
yang dapat dibaca; bukan jaminan batas ukuran disk secara tepat.

## 3. Isi konfigurasi backend

Tambahkan/isikan pada `server/.env` (salin dari `.env.example` jika belum ada):

```dotenv
AETHER_DATABASE=data/aether.sqlite
AETHER_INFLUX_ENABLED=1
AETHER_INFLUX_URL=http://127.0.0.1:8181
AETHER_INFLUX_DATABASE=aether
AETHER_INFLUX_TOKEN=ISI_TOKEN_INFLUXDB
```

`AETHER_DATABASE` tetap menunjuk SQLite. Direktori kerja unit systemd adalah
`/opt/aether/server`, sehingga default file SQLite berada di
`/opt/aether/server/data/aether.sqlite`.

```bash
sudo chown root:aether server/.env
sudo chmod 0640 server/.env
sudo systemctl restart aether-api
journalctl -u aether-api -f
```

Installer `server/deploy/install.sh` menyiapkan API/MQTT seperti sebelumnya.
Setup InfluxDB dilakukan terpisah sesuai panduan ini; installer tidak menginstal
Docker atau membuat token/database secara otomatis. Untuk menjalankan backend
manual tanpa systemd, muat variabel `.env` ke environment proses terlebih dahulu;
Python tidak otomatis membaca file `.env`.

## 4. Periksa penulisan dan baca riwayat

```bash
curl http://127.0.0.1:8000/api/v1/storage
```

Bagian `telemetry` menampilkan `status`, `queued`, `written`, `dropped`,
`last_write` dan kode `last_error`, tanpa token. Setelah ada sampel dan flush
berhasil, status menjadi `ok` dan `written` bertambah. `waiting` berarti belum ada
write sukses; `not_configured` berarti konfigurasi belum lengkap. Angka tersebut
adalah statistik proses saat ini, bukan total historis database.

Endpoint riwayat yang dilindungi API key:

```bash
read -rsp 'API key AETHER: ' AETHER_SETUP_API_KEY
echo
curl -H "Authorization: Bearer $AETHER_SETUP_API_KEY" \
  'http://127.0.0.1:8000/api/v1/telemetry/history?node=nodeB&minutes=60'
unset AETHER_SETUP_API_KEY
```

Pilihan node: `nodeA`, `nodeB`, `nodeC`. Rentang **1–1.440 menit**, maksimal
**2.000 sampel terbaru**, urutan waktu terbaru dahulu. `possibly_truncated=true`
berarti hasil mencapai batas dan mungkin masih ada sampel lain dalam rentang itu;
endpoint ini bukan ekspor penuh database. Jika tabel/data belum ada atau database
tidak dapat diakses, API mengembalikan 503, tanpa memutus endpoint status/insiden.
Riwayat saat ini tersedia sebagai JSON API; grafik historis belum ditambahkan ke
dashboard. Dashboard yang ada tetap menampilkan status terkini dan insiden SQLite.

## Data dan batas operasional

- Tabel InfluxDB: `telemetry`, satu tag `node`. Field mencakup suhu, tegangan,
  CPU/RAM, kode/latensi HTTP, status radio dan flag yang dikirim firmware.
  Field opsional yang belum tersedia tidak dipalsukan sebagai nol.
- Nilai numerik disimpan sebagai float untuk menghindari konflik tipe antara JSON
  integer dan float; boolean tetap boolean. NaN/Infinity tidak ditulis.
- Timestamp memakai waktu penerimaan backend UTC, bukan uptime ESP32. Sinkronkan
  jam Pi. Arsip paling sering satu sampel/detik per ESP; MQTT dan CoAP dalam
  interval yang sama digabungkan dengan memilih sampel pertama. Status terkini
  tetap menerima semua laporan. Metrik host Pi diarsipkan tiap lima detik.
- Writer terpisah memakai antrean RAM **1.024 sampel** dan batch **32 sampel**,
  flush sekitar satu detik, timeout HTTP dua detik. Gangguan jaringan, 401, 429
  dan 5xx dicoba ulang dengan jeda hingga 30 detik, timestamp tetap sama.
- Antrean penuh membuang sampel baru dan menaikkan `dropped`. Batch ditolak karena
  format/schema/ukuran (400/413/422) dibuang agar tidak menyumbat semua sampel.
  Periksa log database untuk penyebabnya. Antrean bersifat sementara: sampel yang
  belum terkirim dapat hilang ketika proses dihentikan atau listrik terputus.
- Tidak ada impor otomatis dari log microSD/LittleFS, telemetri lama di RAM,
  maupun file SQLite. Arsip dimulai dari data yang diterima setelah integrasi aktif.
- InfluxDB mati tidak mengubah jalur recovery atau diagnosis menjadi gangguan host;
  status penyimpanan diperiksa melalui `/api/v1/storage`. MQTT, UART, CoAP,
  dashboard status dan pencatatan insiden SQLite tetap bekerja.
- Retensi 30 hari berbeda dari batas query. Core mempunyai batas jumlah file per
  query (default 432), sehingga rentang panjang dapat ditolak. API AETHER membatasi
  rentang 24 jam dan jumlah hasil, tetapi batas file Core tetap berlaku. Kebutuhan
  analitik jangka panjang perlu dievaluasi terhadap konfigurasi/produk InfluxDB.
- Pemakaian RAM, disk, ketahanan mati daya dan kinerja Pi aktual belum diukur.
  Cadangkan file SQLite dan volume InfluxDB; memonitor ruang disk tetap diperlukan.

## Referensi

- Instalasi/ARM64: https://docs.influxdata.com/influxdb3/core/install/
- Write API v3: https://docs.influxdata.com/influxdb3/core/write-data/http-api/v3-write-lp/
- Query SQL HTTP: https://docs.influxdata.com/influxdb3/core/query-data/execute-queries/influxdb-v3-api/
- Retensi: https://docs.influxdata.com/influxdb3/core/admin/databases/create/
- Batas file query: https://docs.influxdata.com/influxdb3/core/reference/config-options/
