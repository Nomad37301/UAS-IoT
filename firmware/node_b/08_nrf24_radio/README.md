# Bagian 08 — nRF24L01+ Node B

Sketch mandiri ESP32-S3 untuk mengirim telemetri uji dan memvalidasi perintah
radio dari Node C. Pengguna menyetujui **payload 32 byte dengan `reserved[7]`**.
Dokumen master telah diselaraskan; offset field lain tetap sama. Header
`AetherRfPacket.h` dan `AetherState.h` harus dipakai identik di Node C nanti.

## Wiring sesuai kontrak

| nRF24L01+ | Node B ESP32-S3 |
| --- | --- |
| Pin 1 GND | GND |
| Pin 2 VCC | **3V3**, bukan 5 V |
| Pin 3 CE | GPIO 14 |
| Pin 4 CSN | GPIO 10 |
| Pin 5 SCK | GPIO 12 |
| Pin 6 MOSI | GPIO 11 |
| Pin 7 MISO | GPIO 13 |
| Pin 8 IRQ | Kosongkan |

Kapasitor 10–100 uF sesuai dokumen dipasang dekat soket, positif ke 3V3,
negatif ke GND. Gunakan label modul untuk memastikan orientasi pin.

## Konfigurasi transport

- Library **RF24 by TMRh20**, versi tersedia/teruji pada komputer **1.6.2**.
- Channel 76, 250 kbps, PA_LOW, alamat 5 byte.
- TX Node B ke C: `AETH1`; RX perintah dari C: `AETH2`, reading pipe 1.
- CRC radio 16 bit, auto-ACK aktif, static payload, ACK payload dinonaktifkan.
- Retry hardware maksimal tiga kali, dengan jeda 1000 us; tidak ada retry software.
- `startWrite()` memulai TX; `poll()` membaca hasil tanpa menunggu ACK dalam loop.
  Deadline software 50 ms, lalu kembali ke mode listening. Poll wajib berjalan rutin.
- RF24 tetap menjalankan transaksi SPI dan jeda singkat internal, termasuk pulsa
  CE; driver tidak menjamin seluruh pemanggilan bebas dari waktu tunggu library.
- Hasil TX diambil dengan `takeTxResult()` sebelum mengirim paket baru.
- `ACKED` berarti ACK radio, bukan bukti payload valid atau recovery telah dijalankan.
- RX FIFO dipertahankan saat berpindah TX/RX; membaca payload memakai `receive()`.
- Sketch AETHER menggunakan **static payload 32 byte**. Tes generik 16 byte
  hanya memeriksa parameter transport; test paket terpisah memakai 32 byte.
- SPI sketch diatur 4 MHz. Radio half-duplex: ketika TX telemetri, radio tidak
  listening perintah. Node C harus membalik TX/RX pipe terhadap konfigurasi B.

## Upload

1. Instal **RF24 by TMRh20** melalui Library Manager. Versi yang diuji **1.6.2**.
2. Buka `08_nrf24_radio.ino`, pertahankan semua `.ino`, `.h`, `.cpp` dalam folder ini.
3. Pilih **ESP32S3 Dev Module**, paket ESP32 yang diuji **3.3.12**.
4. Gunakan konektor **UART**, port COM dan **USB CDC On Boot: Disabled**;
   sesuaikan flash/PSRAM dengan board fisik.
5. Upload dan buka Serial Monitor **115200 baud**.

Dengan RF24 tersedia pada library directory Arduino normal, jalankan dari akar proyek:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/08_nrf24_radio
```

Konfigurasi CLI lokal `.build` memerlukan tambahan opsi
`--library 'C:\Users\ASUS\Documents\Arduino\libraries\RF24'` untuk library terpasang.
Validasi: tes transport dan paket **PASS**, kompilasi ESP32-S3 **PASS**
(flash 312.114 byte, RAM global 22.312 byte). Upload dan uji fisik belum dilakukan.

## Perintah Serial Monitor

Kirim satu karakter; CR/LF dan whitespace diabaikan.

| Perintah | Hasil |
| --- | --- |
| `t` | Kirim satu paket telemetri uji. |
| `a` | Aktif/nonaktifkan telemetri uji tiap dua detik, awalnya nonaktif. |
| `0`–`4` | Pilih state uji AETHER, tidak langsung mengirim. |
| `r` | Inisialisasi ulang radio ketika tidak BUSY. |
| `i` / `?` | Status / bantuan. |

Program mulai listening tanpa transmisi otomatis. State uji awal 3 (NETWORK_DEAD)
bukan hasil diagnosis. Karena sensor/HTTP/Modbus belum digabungkan, float sensor
di paket uji adalah **NaN**, flags nol dan HTTP code 0. Serial menyatakan
`test_mode:true`; tidak ada flag mode uji tambahan pada format biner.

Seq uint16 bertambah saat TX diterima driver, termasuk jika akhirnya gagal ACK;
BUSY/NOT_READY tidak memakai seq baru. Setelah 65535 kembali ke 0.

```json
{"event":"TELEMETRY_TX","test_mode":true,"seq":0,"result":"ACCEPTED"}
{"event":"TX_RESULT","seq":0,"result":"ACKED"}
```

ACCEPTED berarti TX dimulai, hasil akhir dilaporkan terpisah: ACKED, MAX_RETRIES,
TIMEOUT atau SPI_FAILURE. Status `initialized` menyatakan hasil inisialisasi
terakhir, bukan pemeriksaan kesehatan chip terus-menerus.

## Paket dan perintah masuk

Header memeriksa ukuran 32 byte dan offset pada waktu kompilasi. Float IEEE-754
32 bit serta nilai multi-byte memakai little-endian. Pengirim mengosongkan
reserved tujuh byte; penerima mengabaikannya.

Command harus memiliki magic 0xAE, msg_type 2, state 0–4, hanya empat bit flags
rendah, dan cmd 1=TRIGGER_RECOVERY atau 2=MUTE_ALARM. Command tidak sesuai ditolak
beserta alasannya. Nilai sensor pada command tidak dipakai. Seq identik dengan
command valid terakhir ditandai duplikat; ini hanya deteksi pengulangan terakhir,
bukan pengurutan monoton atau pencegahan replay lintas sesi.

```json
{"event":"RADIO_COMMAND","seq":12,"action":"TRIGGER_RECOVERY","executed":false}
```

**Tahap ini memvalidasi dan mencatat command, belum mengeksekusi aktuator.**
Integrasi nanti menghubungkan cmd 1 ke `RecoveryRelay::trigger()` bagian 06
(termasuk cooldown) dan cmd 2 ke `ActiveBuzzer::setMuted()` bagian 03/04.
ACK hardware diberikan sebelum validasi aplikasi; ACK bukan bukti relay aktif,
buzzer muted atau service sudah pulih.

## Integrasi dengan Node C dan uji fisik

Node C harus menggunakan channel 76, 250 kbps, PA_LOW, address width 5,
static payload 32 byte, CRC 16 bit, auto-ACK aktif, dynamic/ACK payload nonaktif.
**Node C TX=AETH2, RX pipe 1=AETH1**. Node C belum dibuat; tanpa penerima,
MAX_RETRIES adalah hasil yang diharapkan.

1. Jalankan sketch; jika inisialisasi gagal, periksa wiring/suplai lalu `r`.
2. Tanpa C, kirim `t`: pastikan mendapat MAX_RETRIES dan CLI tetap responsif.
3. Setelah radio C tersedia, kirim `t`, cek ACK dan penerimaan tepat 32 byte di C.
   NaN harus diperlakukan sebagai metrik belum tersedia.
4. Aktifkan `a`, pilih state `0`–`4`, dan periksa paket di C.
5. Dari C kirim cmd 1/2: B mencatat RADIO_COMMAND dan `executed:false`.
   Ulangi seq yang sama untuk RX_DUPLICATE; cmd tidak sah untuk RX_REJECTED.
6. Uji aksi fisik baru dilakukan saat integrasi aktuator.

Semua API driver dipanggil dari satu loop pemilik. Pembacaan RX dibatasi tiga
paket tiap service. Ketepatan deadline memerlukan polling rutin. CRC radio
melindungi frame; validasi aplikasi memeriksa isi command. Kedua node wajib
memakai panjang payload identik. Pada integrasi firmware, gunakan satu salinan
header bersama agar definisi tidak ganda.

Upload mengganti program yang berjalan. Radio Node C, integrasi sensor/aktuator,
upload, uji jangkauan dan uji radio fisik belum dilakukan.

API diperiksa pada source library terpasang dan
[dokumentasi resmi RF24](https://rf24.readthedocs.io/en/stable/classRF24.html).
