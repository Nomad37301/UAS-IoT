# Bagian 07 — UART1 / Modbus RTU master Node B

Sketch mandiri ESP32-S3 untuk kontrak AETHER: **9600 baud, 8N1, slave ID 1**.
Driver membentuk dan memeriksa frame RTU langsung; tidak memerlukan library
Modbus tambahan. Bagian ini belum membuat slave/daemon Raspberry Pi.

## Wiring

| Node B | Raspberry Pi 5 |
| --- | --- |
| GPIO 17 / UART1 TX | Pin fisik 10 / GPIO 15 RXD |
| GPIO 18 / UART1 RX | Pin fisik 8 / GPIO 14 TXD |
| GND | Pin fisik 6 / GND |

Sambungan ini UART logika **3,3 V point-to-point**, sesuai jumper proyek, bukan
port RS-232 atau bus RS-485. Tidak memakai pin DE/RE atau transceiver RS-485.
Jangan menghubungkan sinyal UART 5 V atau level tegangan RS-232 langsung.
Gunakan USB UART Node B untuk upload/Serial Monitor, terpisah dari UART1 ini.
Pin GPIO 17 Node B adalah TX; pin BCM 17 Raspberry Pi adalah jalur relay lain.

## Kontrak register

FC03 membaca **lima holding register sekaligus**, mulai alamat zero-based 0:

| Register | Field | Konversi |
| --- | --- | --- |
| 0x0000 | `cpu_temp_c` | uint16 / 10, misalnya 492 menjadi 49,2 °C |
| 0x0001 | `cpu_ram_pct` | 0–100 % |
| 0x0002 | `cpu_load_pct` | 0–100 % |
| 0x0003 | `service_running` | 0=false, 1=true |
| 0x0004 | `uptime_s_low16` | Detik low 16-bit, berulang setelah 65535 |

FC06 hanya menulis **1 ke register 0x0005**, meminta restart target service.
Ini berbeda dari relay recovery bagian 06. Request dalam hex:

```text
FC03: 01 03 00 00 00 05 85 C9
FC06: 01 06 00 05 00 01 58 0B
```

Data register dikirim high byte dahulu; CRC dikirim low byte dahulu.
CRC-16 memakai initial 0xFFFF dan reflected polynomial 0xA001.

## Upload dan operasi

1. Buka `07_modbus_master.ino`; semua `.ino`, `.h`, `.cpp` tetap dalam folder ini.
2. Pilih **ESP32S3 Dev Module**, paket ESP32 yang diuji **3.3.12**.
3. Gunakan konektor **UART**, port COM board dan **USB CDC On Boot: Disabled**.
   Flash/PSRAM disesuaikan dengan board fisik.
4. Upload, buka Serial Monitor **115200 baud**. Pembacaan FC03 otomatis tiap
   dua detik saat driver tersedia; tidak ada write/restart otomatis.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/07_modbus_master
```

Validasi komputer proyek: tes driver host **PASS**, kompilasi ESP32-S3 **PASS**
(flash 276.697 byte, RAM global 21.696 byte pada konfigurasi tersebut).
Upload, uji UART/slave fisik dan restart service belum dilakukan.

Upload mengganti program yang berjalan di board; sketch komponen sebelumnya tetap
tersimpan. Sensor, buzzer, tombol, RGB, relay dan jaringan belum digabungkan.

Kirim satu huruf kecil; newline diabaikan:

| Perintah | Hasil |
| --- | --- |
| `p` | Baca lima register sekarang. |
| `r` | Tulis perintah restart service FC06 satu kali. |
| `a` | Aktifkan/nonaktifkan polling FC03 otomatis. |
| `?` | Bantuan. |

Permintaan baru ditolak `BUSY` selama transaksi/hasil belum selesai diambil.
Tidak ada antrean dan tidak ada retry otomatis pada transaksi, terutama FC06.
`ACCEPTED` berarti diterima oleh state machine, belum berarti data terkirim
atau diterima slave. Jika UART gagal inisialisasi, request menghasilkan `NOT_STARTED`.

Contoh hasil FC03 (angka ilustrasi):

```json
{"system":"AETHER","node":"nodeB","component":"MODBUS_MASTER","operation":"READ_METRICS","modbus_ok":true,"error":"OK","exception_code":0,"cpu_temp_c":49.2,"cpu_ram_pct":34,"cpu_load_pct":20,"service_running":true,"uptime_s_low16":1234,"restart_ack":false}
```

Saat FC03 gagal, metrik menjadi `null` agar sampel lama tidak dianggap baru.
FC06 sukses juga tidak membawa metrik. `restart_ack=true` berarti echo write
sesuai request diterima, **bukan bukti service sudah sehat**. Timeout FC06 berarti
hasil tidak diketahui: slave mungkin sudah menjalankan write namun respons hilang.

## State machine dan validasi

- Satu UART dimiliki eksklusif oleh satu driver, semua metode dari satu loop.
  Panggil `poll(micros())` rutin, lalu ambil hasil dengan `takeResult()`.
- Tidak memakai `delay()`, `readBytes()` atau loop menunggu respons di driver.
  Tiap poll memproses maksimal 64 byte. Buffer respons tetap 32 byte, cukup
  untuk subset kontrak (FC03=15 byte, FC06=8 byte, exception=5 byte).
- Menunggu bus terlihat diam **5 ms** sebelum TX, membuang byte lama yang
  terlihat saat idle/menunggu. Memeriksa ruang TX sebelum mengantrekan 8 byte.
- Guard **14 ms** setelah TX diantrekan mencakup waktu 8 byte pada 9600 8N1
  serta quiet gap, termasuk ketika write gagal sebagian.
- Timeout tunggu bus/TX **500 ms**. Setelah TX diantrekan, timeout respons
  **500 ms** lagi, termasuk waktu pengiriman UART. Tidak ada retry tanpa batas.
- Frame diproses setelah RX terlihat diam 5 ms; CRC, panjang, slave, fungsi,
  jumlah byte, exception dan echo FC06 diperiksa sebelum menerima data.
  RAM/load di luar 0–100 atau status service di luar 0/1 ditolak.
- Selisih waktu unsigned menangani rollover `micros()`. Jika polling terhenti
  hingga deadline, driver mengambil hasil konservatif TIMEOUT, meskipun ada
  byte di buffer yang belum diproses.

## Batas kompatibilitas

Konfigurasi **8N1 sengaja mengikuti kontrak proyek**. Panduan serial Modbus
standar menggunakan even parity atau dua stop bit saat tanpa parity; jangan
mengklaim konfigurasi proyek sebagai profil serial standar penuh. Slave Node A
harus memakai 9600 **8N1 yang sama**.

Timing RX di sini diamati dari buffer UART melalui polling; driver tidak
mendapat timestamp setiap byte di wire dan tidak menjamin validasi ketat jeda
antar-karakter t1.5. Gap framing 5 ms dan CRC digunakan pada jalur pendek proyek.
Polling yang lambat bisa menyatukan byte yang sebenarnya terpisah gap di wire;
persyaratan timing RTU penuh membutuhkan pengukuran event/timer RX hardware.

RTU tidak memiliki transaction ID: respons lama yang baru tiba setelah request
identik dikirim tidak selalu bisa dibedakan. Byte lama yang sudah terlihat
sebelum TX dibuang; tidak ada jaminan freshness terhadap respons terlambat tak
terbatas. Satu hasil `modbus_ok` bukan diagnosis lengkap kesehatan host.

## Uji perangkat

1. Siapkan slave Node A dengan alamat/register di atas, UART header aktif dan
   console login tidak memakai port tersebut. Dokumen master menetapkan
   `/dev/ttyAMA0`; pemetaan port Pi fisik perlu diverifikasi saat bagian Node A.
2. Jalankan polling. Bandingkan suhu/RAM/load/status/uptime dengan data slave.
3. Kirim `a` untuk menghentikan polling otomatis, lalu `p` untuk satu pembacaan.
4. Ketika slave siap menangani restart, kirim `r` satu kali. Verifikasi ACK,
   lalu `p` dan cek service secara langsung untuk memastikan pemulihan.
5. Matikan daya sebelum melepas sambungan. Jalankan tanpa slave: hasil harus
   TIMEOUT dan metrik null; sketch tetap menerima perintah.

Slave Node A belum dibuat pada bagian ini, sehingga tanpa slave respons timeout
adalah hasil yang diharapkan. Tes host tersedia pada `tests/modbus_master/`.

Referensi: dokumen master AETHER, [spesifikasi aplikasi Modbus](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf),
[panduan Modbus serial](https://www.modbus.org/file/secure/modbusoverserial.pdf),
dan [API UART Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html).
