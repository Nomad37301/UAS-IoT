# Progres implementasi AETHER

Instruksi terbaru pengguna: lanjutkan seluruh bagian sampai selesai tanpa menunggu
per komponen. Implementasi software tiga node sudah tersedia. Dokumen master dan
skematik tetap menjadi kontrak pin/protokol; uji perangkat fisik belum dilakukan.

| Urutan | Komponen | Status |
| --- | --- | --- |
| 01 | Node B — sensor suhu IR GY-906 / MLX90614 | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 02 | Node B — sensor tegangan ADC GPIO 4 | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 03 | Node B — active buzzer GPIO 15 | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 04 | Node B — tombol BOOT/mute GPIO 0 | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 05 | Node B — onboard RGB LED GPIO 48 | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 06 | Node B — relay recovery GPIO 16, aktif HIGH | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 07 | Node B — UART1 / Modbus RTU master | Selesai: tes host dan kompilasi ESP32-S3 lulus; uji perangkat fisik belum dilakukan. |
| 08 | Node B — nRF24L01+ | Selesai: payload 32 byte disetujui; tes transport/paket dan kompilasi lulus; uji fisik belum dilakukan. |
| 09 | Node B — LittleFS blackbox logger | Selesai: tes driver/parser JSON dan kompilasi lulus; uji flash fisik belum dilakukan. |
| 10 | WiFi, HTTP probe B/C, MQTT telemetry/LWT/command | Diimplementasikan dalam NetworkLink; kedua firmware final lulus kompilasi. |
| 11 | CoAP server B, collector A, resource subset LwM2M/IPSO | Codec lintas C++/Python dan collector UDP localhost lulus tes. |
| 12 | Diagnosis dan recovery otomatis terintegrasi B | Matriks diagnosis, timing, batas tiga siklus dan rollover lulus tes host. |
| 13 | Node C lengkap: radio, OLED, tiga LED, tombol GPIO33 dan CLI | Firmware ESP32 DevKit V1 lulus kompilasi; uji fisik belum dilakukan. |
| 14 | Node A: target service, API, MQTT, Modbus slave, GPIO daemon, SQLite | 11 tes server termasuk HTTP target dan UDP lokal lulus; GPIO/serial Pi belum diuji fisik. |
| 15 | Dashboard REST dan fault injection | Tes browser desktop/mobile lulus, tiga alur simulasi diverifikasi. |
| 16 | Deployment, konfigurasi dan paket source/library | Panduan README, systemd, broker config, installer dan packaging tersedia. |

Kode dan petunjuk tersedia pada `firmware/node_b/01_gy906/`,
`firmware/node_b/02_voltage_adc/`, `firmware/node_b/03_active_buzzer/`,
`firmware/node_b/04_boot_mute/`, `firmware/node_b/05_rgb_status/`,
`firmware/node_b/06_recovery_relay/`, `firmware/node_b/07_modbus_master/`,
`firmware/node_b/08_nrf24_radio/` dan `firmware/node_b/09_littlefs_blackbox/`.
Masing-masing adalah sketch pengujian mandiri;
penggabungan dilakukan pada tahap integrasi.
Gunakan `firmware/node_b/AetherNodeB`, `firmware/node_c/AetherNodeC`, library
`firmware/libraries/AetherCore` dan `server/` untuk sistem akhir. Catatan bagian
01–09 di bawah menjelaskan snapshot uji mandiri, bukan status integrasi akhir.
Keputusan radio tetap **32 byte dengan reserved[7]**, relay tetap **aktif HIGH**.
Pengaturan SSID/password/IP serta deployment/upload hardware diisi pengguna
pada perangkat; tidak ada perangkat fisik yang diakses saat pengerjaan ini.

## Batas lingkup bagian 01

Hanya driver GY-906, pembacaan suhu object/ambient dan keluaran diagnostik USB UART.
Ambang overheat dan diagnosis sistem akan ditentukan saat tahap logika diagnosis;
dokumen master belum menetapkan nilai ambang overheat.

## Validasi bagian 01

- Tes driver C++11 dengan g++: **PASS**, termasuk vektor PEC datasheet,
  konversi suhu, error bus/sensor, pembatalan data lama dan pemulihan.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **301.825 byte (23%)** dan RAM global
  **23.048 byte (7%)** pada konfigurasi build tersebut.
- Upload dan pengujian sensor fisik belum dilakukan. Langkah pengujian dan
  pemilihan board/port tersedia dalam README komponen.
- Hasil build lokal dan konfigurasi CLI mesin ini berada di `.build/`,
  dikecualikan dari version control.

## Batas lingkup bagian 02

Driver ADC GPIO 4, pembagi tegangan 1:5, rata-rata 16 sampel mV terkalibrasi,
kalibrasi gain/offset, indikator tegangan <4,5 V dan keluaran diagnostik USB UART.
Tidak mengaktifkan aktuator. Sampel raw untuk deteksi clipping dan sampel mV
adalah konversi yang berbeda. Nilai nol belum dapat membedakan suplai mati,
kabel putus atau kegagalan internal API ADC; batas tersebut dijelaskan pada README.

## Validasi bagian 02

- Tes driver C++11 dengan g++: **PASS**, mencakup konfigurasi ADC, rata-rata,
  konversi 1:5, batas 4,5 V, kalibrasi, clipping, pembatalan data dan pemulihan.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **291.345 byte (22%)** dan RAM global
  **22.264 byte (6%)** pada konfigurasi build tersebut.
- Upload dan uji fisik belum dilakukan. Perbandingan dengan multimeter dan uji
  tegangan rendah tersedia pada README komponen.

## Batas lingkup bagian 03

Driver active buzzer HIGH pada GPIO 15, pola Test/Warning/Critical berbasis
`millis()`, mute/unmute yang mempertahankan pola, CLI satu huruf dan status JSON.
Boot program dimulai diam. Tombol BOOT, diagnosis, radio, MQTT dan relay belum
diintegrasikan; mute tahap ini diuji lewat Serial Monitor.
Rating listrik buzzer belum tercantum di dokumen master dan perlu diperiksa
sebelum menguji wiring langsung ke GPIO sesuai panduan komponen.

## Validasi bagian 03

- Tes driver C++11 dengan g++: **PASS**, mencakup kondisi awal diam, timing pola,
  mute/unmute, kedaluwarsa Test saat muted, tidak ada pulsa HIGH sesaat ketika
  mute, update terlambat, rollover waktu dan pola tidak dikenal.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **303.054 byte (23%)** dan RAM global
  **22.144 byte (6%)** pada konfigurasi build tersebut.
- Upload dan uji bunyi fisik belum dilakukan. Panduan uji tersedia pada README.

## Batas lingkup bagian 04

Driver tombol BOOT GPIO 0, aktif LOW dengan INPUT_PULLUP dan debounce polling
30 ms saat tekan/lepas. Setiap penekanan stabil memicu mute sekali; tahan tidak
mengulang dan tekan berikutnya tidak melakukan toggle. Driver menunggu kondisi
lepas stabil sebelum menerima penekanan pertama. Unmute memakai perintah serial.
Contoh memakai salinan driver buzzer bagian 03 tanpa perubahan agar sketch tetap
mandiri; sensor, RGB, relay dan protokol jaringan belum digabungkan.

## Validasi bagian 04

- Tes driver C++11 dengan g++: **PASS**, mencakup pull-up, bounce tekan/lepas,
  batas debounce, startup LOW, tahan lama, re-arm, rollover waktu dan integrasi
  mute tombol ke buzzer, termasuk penekanan berulang saat sudah muted.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **303.642 byte (23%)** dan RAM global
  **22.160 byte (6%)** pada konfigurasi build tersebut.
- Upload dan uji tombol/bunyi fisik belum dilakukan. Penggunaan GPIO 0 sebagai
  pin strapping saat reset dijelaskan pada README komponen.

## Batas lingkup bagian 05

Driver WS2812 onboard GPIO 48 menggunakan API RGB/RMT core ESP32 dan urutan GRB.
Kode diagnosis 0–4 mengikuti kontrak master. Warna hijau/kuning/merah mengikuti
kontrak; durasi kedip merupakan pilihan implementasi. Mode OFF/TEST terpisah dari
enum diagnosis. Program dimulai OFF, tersedia test RGB terbatas, CLI dan JSON.
Status dipilih manual; diagnosis otomatis dan komponen lain belum digabungkan.

## Validasi bagian 05

- Tes driver C++11 dengan g++: **PASS**, mencakup kontrak kode/GPIO48/GRB,
  warna, batas fase, status berulang, test selesai OFF, update terlambat,
  rollover waktu, state tidak dikenal dan intensitas.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **300.130 byte (22%)** dan RAM global
  **22.096 byte (6%)** pada konfigurasi build tersebut.
- Upload dan uji warna fisik belum dilakukan. Penggunaan API RGB bersifat
  sinkron pada pengiriman frame dan tidak menyediakan feedback keberhasilan;
  batasan serta pemeriksaan varian board dijelaskan pada README komponen.

## Batas lingkup bagian 06

Driver relay GPIO 16 dengan polaritas **aktif HIGH yang dikonfirmasi pengguna**.
Pulsa nominal 500 ms, cooldown 30 detik setelah pelepasan, pembatalan, penolakan
perintah BUSY/COOLDOWN tanpa antrean, serta CLI dan JSON. Durasi dipilih sebagai
keputusan implementasi karena belum ditetapkan dalam dokumen master.
Program dimulai OFF; belum ada recovery otomatis atau integrasi protokol.
`update()` harus berjalan rutin: pulsa bisa lebih panjang jika loop terhenti;
cooldown dihitung dari perintah OFF aktual. Cooldown RAM hilang ketika reset.

## Validasi bagian 06

- Tes driver C++11 dengan g++: **PASS**, mencakup GPIO16, kondisi awal OFF,
  polaritas HIGH/LOW generik, batas pulsa 499/500 ms, BUSY, cooldown 30 detik,
  pembatalan tanpa mengulang timer, pelepasan terlambat, tanpa antrean dan rollover.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **303.194 byte (23%)** dan RAM global
  **22.152 byte (6%)** pada konfigurasi build tersebut.
- Upload, uji kontak fisik dan recovery Raspberry Pi belum dilakukan. Daemon
  recovery Node A belum dibuat; relay ke pin 11 adalah input daemon, bukan
  sambungan langsung ke pin reset hardware.

## Batas lingkup bagian 07

UART1 TX17/RX18, 9600 8N1, slave 1. FC03 membaca register 0–4, FC06 menulis
register 5=1 untuk restart service manual. Driver memakai state machine polling,
CRC-16, buffer tetap, pengecekan frame/exception/echo, quiet gap 5 ms,
guard TX 14 ms dan timeout 500 ms per fase tunggu bus/respons. Tidak ada retry
otomatis write atau antrean permintaan BUSY. Data gagal tidak memakai sampel lama.
Sketch polling FC03 tiap 2 detik; belum menggabungkan komponen lain.

## Validasi bagian 07

- Tes driver C++11 dengan g++: **PASS**, mencakup vektor CRC/request, dekode
  register, validasi frame/data, exception, FC06 echo, timeout, respons
  terfragmentasi, byte lama sebelum TX, pembatalan data lama, write parsial,
  kapasitas/guard TX dan rollover waktu.
- Kompilasi Arduino CLI: **PASS**, paket `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **276.697 byte (21%)** dan RAM global
  **21.696 byte (6%)** pada konfigurasi build tersebut.
- Uji UART fisik/slave Node A/restart service belum dilakukan. Slave Node A
  belum dibuat; tanpa slave hasil timeout adalah normal.
- 8N1 mengikuti kontrak proyek; berbeda dari profil serial Modbus standar
  even parity atau no parity dengan dua stop bit. RX melalui buffer polling
  belum menjamin pemeriksaan t1.5 per byte di wire. Batas timing, freshness RTU
  dan arti ACK restart dijelaskan pada README; tidak mengklaim compliance penuh.

## Batas lingkup bagian 08

Driver transport SPI/RF24 Node B, channel 76, 250 kbps, PA_LOW, TX AETH1/RX AETH2,
payload static 32 byte, CRC radio 16 bit. Pengguna menyetujui perluasan reserved
dari tiga menjadi tujuh byte. Header memeriksa ukuran, offset dan tipe float.
Sketch mengirim telemetri uji manual/tiap dua detik dan memvalidasi command masuk.
Sensor belum diintegrasikan: float sensor NaN, flags nol, HTTP 0; state dipilih
manual. Command dicatat dengan executed=false; relay/buzzer digabungkan nanti.
Nomor command identik dengan command terakhir dideteksi sebagai duplikat.

## Validasi bagian 08

- Tes transport dan paket C++11 dengan g++: **PASS**, termasuk konfigurasi radio,
  init gagal, ukuran, hasil TX asinkron, FIFO RX, timeout/rollover, layout 32 byte,
  byte golden little-endian, reserved[7], validasi command dan duplikat seq.
- Kompilasi sketch final ESP32-S3: **PASS**, core 3.3.12, RF24 1.6.2,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian hasil build: flash **312.114 byte (23%)**, RAM global
  **22.312 byte (6%)** pada konfigurasi build tersebut.
- Upload, radio Node C, uji fisik/jangkauan dan integrasi sensor/aktuator belum
  dilakukan. ACK hardware tidak menjamin command dieksekusi aplikasi.

## Batas lingkup bagian 09

Logger JSON Lines pada LittleFS, dua file current/previous masing-masing 32 KiB,
session diagnostik dan seq, escaping JSON, rotasi, pemisahan ekor terputus dan
penanganan error IO. Mount tanpa auto-format. Format seluruh partisi hanya tersedia
melalui perintah serial eksplisit FORMAT; tidak dijalankan ke perangkat saat pengerjaan.
CLI mencatat event uji, heartbeat opsional 10 detik, dump bertahap, status/remount.
Belum digabungkan dengan insiden diagnosis atau komponen/protokol lain.

## Validasi bagian 09

- Tes driver C++11 dengan g++: **PASS**, mencakup escaping, batas record/file,
  dua rotasi, preservasi isi, ekor terputus, partial write dan kegagalan IO.
- Parser JSON Python independen: **PASS**, termasuk quote/backslash, karakter
  kontrol, panjang message maksimum dan field numerik.
- Kompilasi Arduino CLI: **PASS**, core `esp32:esp32` versi `3.3.12`,
  FQBN `esp32:esp32:esp32s3:CDCOnBoot=default`.
- Pemakaian build: flash **317.261 byte (24%)**, RAM global **21.832 byte (6%)**.
- Upload, persistensi flash fisik dan uji mati daya belum dilakukan. IO sinkron;
  rotasi dua file bukan transaksi atomik gabungan, dan API flush/close tidak
  mengembalikan status error. Batasan serta partisi/format dijelaskan pada README.

## Perubahan penyimpanan firmware final Node B — microSD

Atas permintaan pengguna setelah foto board ESP32-S3 N16R8 diberikan, firmware
`AetherNodeB` memakai slot microSD bawaan melalui SDMMC 1-bit: CLK39, CMD38,
D0=40 (mapping referensi Freenove; varian board pengguna perlu diuji langsung).
Log event/insiden ditulis ke dua file JSONL di root kartu FAT32, masing-masing
maksimal 32 KiB. Mount tidak memformat kartu dan tidak fallback ke LittleFS.
Kegagalan mount/IO dicetak ke Serial; restart setelah kartu diperbaiki.
Sketch komponen 09 tetap menjadi uji LittleFS internal, terpisah dari firmware final.

## Varian final Node B tanpa microSD — 19 November 2026

Ditambahkan `firmware/node_b/AetherNodeB_InternalFlash/` sebagai sketch final
mandiri, dengan Config.h dan Secrets.h.example sendiri. Penyimpanan memakai
LittleFS flash internal tanpa auto-format; log membutuhkan partisi spiffs minimal
128 KiB dan inisialisasi melalui sketch 09 jika filesystem belum ada.
Logika sensor, jaringan, UART, radio dan recovery identik dengan versi microSD.
README menjelaskan pemilihan varian; script build memeriksa kedua varian B dan C.

## Catatan untuk integrasi berikutnya

- Kontrak radio telah diperbaiki menjadi **32 byte dengan reserved[7]**, disetujui
  pengguna. Node C wajib memakai header `AetherRfPacket.h`/`AetherState.h` identik
  dengan bagian 08; empat byte tambahan hanya memperluas reserved, offset data tetap.
- Relay pada GPIO 17 Raspberry Pi bergantung pada daemon yang berjalan di Pi.
  Jalur ini merupakan permintaan recovery software lewat kontak fisik. Pemulihan
  ketika kernel/host sepenuhnya macet memerlukan jalur reset/power tersendiri;
  jangan mengasumsikan GPIO input biasa adalah pin reset hardware.
