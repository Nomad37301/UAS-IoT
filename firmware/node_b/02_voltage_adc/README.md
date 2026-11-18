# Bagian 02 — Sensor tegangan ADC pada Node B

Sketch mandiri untuk sensor pembagi tegangan 1:5 pada ESP32-S3. Buka folder ini
sebagai sketch terpisah dari bagian 01; upload mengganti sketch yang sedang
berjalan di board. Penggabungan komponen dilakukan pada tahap integrasi.
Semua file `.ino`, `.h`, `.cpp` dalam folder ini harus tetap bersama.

## Wiring sesuai dokumen master

| Terminal sensor | Sambungan |
| --- | --- |
| Baut input VCC | Raspberry Pi pin fisik 2, jalur 5 V yang diukur |
| Baut input GND | Common GND Node B dan Raspberry Pi pin fisik 6 |
| Header S | ESP32-S3 GPIO 4 / ADC1_CH3 |
| Header - | GND Node B |
| Header + tengah | Kosongkan |

Node B diberi daya tersendiri melalui USB UART. Input VCC sensor adalah **jalur
ukur**, bukan sambungan untuk menyatukan rel 5 V USB Node B dan Raspberry Pi.
Matikan daya sebelum mengubah wiring.

**Batas penggunaan:** bagian ini untuk mengukur jalur **5 V** Raspberry Pi.
Label modul “0–25 V” bukan batas aman untuk GPIO ESP32-S3. Dengan pembagi 1:5,
25 V menghasilkan 5 V pada S dan tidak boleh diberikan ke GPIO. Pada jalur
5 V, sinyal S nominal sekitar 1 V. Jangan mengandalkan deteksi clipping software
untuk melindungi input dari tegangan berlebih.

## Upload dan menjalankan

1. Buka `02_voltage_adc.ino` di Arduino IDE.
2. Pilih **ESP32S3 Dev Module**, paket **esp32 by Espressif Systems**.
   Versi paket pada komputer proyek: **3.3.12**.
3. Gunakan konektor **UART** board sesuai gambar proyek dan port COM yang muncul.
   Pilih **USB CDC On Boot: Disabled** untuk konektor USB-UART ini.
   Atur flash dan PSRAM sesuai varian board fisik.
4. Upload. Jika perlu, tahan BOOT, tekan-lepas RESET, lalu lepas BOOT saat koneksi upload.
5. Buka Serial Monitor **115200 baud**. Laporan JSON muncul setiap satu detik.

Alternatif kompilasi dari akar proyek setelah paket ESP32 terpasang:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/02_voltage_adc
```

Validasi komputer proyek: tes driver host **PASS**, kompilasi ESP32-S3 **PASS**
(flash 291.345 byte, RAM global 22.264 byte pada konfigurasi tersebut).
Upload dan pengujian perangkat fisik belum dilakukan.

## Pembacaan dan kalibrasi

- ADC 12 bit, attenuasi per pin `ADC_11db`; rentang ukur ESP32-S3 sampai sekitar
  3100 mV pada GPIO, sesuai dokumentasi Espressif.
- Tiap laporan mengambil rata-rata 16 konversi `analogReadMilliVolts()`.
  Tidak ada jeda satu detik di dalam driver atau retry tanpa batas.
- Ada 16 konversi raw terpisah untuk memeriksa clipping pada nilai 4095.
  Nilai raw dan mV berasal dari konversi yang berbeda.
- Rumus default persis kontrak proyek: `voltage_v = (adc_mv / 1000) * 5`.
- Kalibrasi opsional: `voltage_v = (adc_mv / 1000) * 5 * gain + offset_v`.
  Parameter berada di awal `.ino`; default `gain=1`, `offset_v=0`.
- `low_voltage=true` jika hasil **< 4,5 V**. Tepat 4,5 V tidak termasuk rendah.
  Ini indikator diagnostik tanpa hysteresis, belum memicu relay atau diagnosis sistem.
- Clipping, nilai mV >= 3100, konfigurasi kalibrasi tidak sah, dan hasil negatif
  membatalkan laporan: `valid=false`, data numerik dan `low_voltage` menjadi `null`.

Contoh bentuk keluaran (angka ilustrasi):

```json
{"system":"AETHER","node":"nodeB","component":"VOLTAGE_ADC","seq":1,"uptime_ms":1000,"valid":true,"adc_mv":1008.00,"voltage_v":5.040,"low_voltage":false,"error":"OK"}
```

Untuk kalibrasi satu titik, gunakan suplai stabil dan ukur jalur yang sama memakai
multimeter. Dengan offset tetap nol:

```text
gain_baru = gain_lama * tegangan_multimeter / voltage_v_sekarang
contoh: 1.0 * 5.05 / 4.95 = 1.020202
```

Masukkan hasil ke `kVoltageGain`, upload ulang dan bandingkan kembali. Kalibrasi
satu titik mengoreksi skala, bukan semua nonlinieritas ADC. Jangan mengkalibrasi
dari nilai nol, laporan gagal, atau suplai yang berubah-ubah.

## Uji perangkat

1. Ukur jalur 5 V dengan multimeter, bandingkan dengan JSON dan kalibrasi jika perlu.
2. Untuk menguji drop, gunakan suplai uji terpisah yang diketahui nilainya pada
   input sensor, common GND, **tanpa Raspberry Pi terhubung ke suplai uji**.
   Bandingkan sekitar 5 V dan 4,4 V: flag rendah harus berubah sesuai hasil ukur.
3. Pada input 0 V yang benar-benar dihubungkan ke GND, keluaran diharapkan
   mendekati nol dengan `low_voltage=true`.
4. Kembalikan wiring pengukuran Raspberry Pi dan pastikan laporan kembali normal.

API ADC Arduino tidak menyediakan status keberhasilan konversi melalui nilai
balik ini; core dapat mengembalikan nol ketika gagal. Pembacaan nol juga dapat
berarti suplai mati, S di-ground-kan, atau wiring terputus. `valid=true` berarti
lolos pemeriksaan driver, **bukan bukti sensor tersambung**. Jangan menguji input
dengan melepas S dan menganggap pin mengambang sebagai tegangan sah. Pengambilan
burst dan interval satu detik belum menjamin deteksi drop yang sangat singkat.

## Referensi

Pin GPIO 4, rasio 1:5, wiring dan ambang <4,5 V berasal dari dokumentasi master
dan skematik di akar proyek. API dan rentang attenuasi mengikuti
[dokumentasi ADC resmi Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html).
