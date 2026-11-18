# Bagian 05 — RGB LED status Node B

Sketch mandiri untuk **onboard WS2812, GPIO fisik 48**, sesuai kontrak proyek.
Tidak memerlukan kabel atau LED tambahan. Driver memakai `rgbLedWriteOrdered()`
dari Arduino-ESP32, urutan warna **GRB**, dengan peripheral RMT core.
Tidak memerlukan library NeoPixel tambahan.

## Upload

1. Buka `05_rgb_status.ino` dan pertahankan semua `.ino`, `.h`, `.cpp` dalam folder ini.
2. Pilih **ESP32S3 Dev Module**, paket **esp32 by Espressif Systems 3.3.12**
   seperti versi yang diuji pada komputer proyek.
3. Gunakan konektor **UART**, port COM board, **USB CDC On Boot: Disabled**.
   Sesuaikan flash/PSRAM dengan varian board fisik.
4. Upload, lalu buka Serial Monitor **115200 baud**. LED dimulai OFF.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/05_rgb_status
```

Validasi komputer proyek: tes driver host **PASS**, kompilasi ESP32-S3 **PASS**
(flash 300.130 byte, RAM global 22.096 byte pada konfigurasi tersebut).
Upload dan pengujian warna pada board fisik belum dilakukan.

Upload mengganti program yang sedang berjalan di board. Bagian terdahulu tetap
berupa sketch terpisah. Bagian ini hanya menampilkan status yang diperintahkan;
diagnosis otomatis, sensor, tombol, buzzer, relay dan jaringan belum digabungkan.

## Perintah dan pemetaan status

Kirim satu karakter di Serial Monitor. CR/LF dan whitespace diabaikan.

| Perintah | Kode / status | Indikator |
| --- | --- | --- |
| `0` | 0 / HEALTHY | Hijau tetap |
| `1` | 1 / EXT_ISOLATED | Kuning tetap |
| `2` | 2 / SERVICE_DOWN | Merah tetap |
| `3` | 3 / NETWORK_DEAD | Kuning 500 ms, merah 500 ms, berulang |
| `4` | 4 / HW_ANOMALY | Merah ON 150 ms, OFF 150 ms, berulang |
| `t` | Test warna | Merah, hijau, biru masing-masing 300 ms, lalu OFF |
| `s` | OFF | Matikan LED |
| `i` / `?` | Diagnostik | Status / bantuan |

Kode 0–4 dan warna dasar mengikuti dokumen master; durasi pola dan test adalah
pilihan implementasi. OFF/TEST merupakan mode tampilan, bukan nilai tambahan
`AetherState`. Setelah test, mode kembali OFF tanpa mengembalikan status sebelumnya.
Mengirim status yang sama berulang tidak mengulang fase kedip. Perintah `t`
selalu memulai test dari awal. State tidak dikenal pada API membatalkan tampilan
ke OFF dan `setState()` mengembalikan `false`.

Contoh keluaran:

```json
{"system":"AETHER","node":"nodeB","component":"RGB_STATUS","uptime_ms":1000,"mode":"STATE","state_code":0,"state":"HEALTHY","red":0,"green":32,"blue":0}
```

Saat OFF/TEST, `state_code` dan `state` bernilai `null`. Field `red/green/blue`
adalah nilai kanal yang diminta ke driver, bukan feedback cahaya fisik.

## Implementasi dan batasan

- Intensitas kanal default **32 dari 255**, diatur melalui `kBrightness` pada `.ino`.
  Kuning menggunakan merah dan hijau pada intensitas yang sama.
- Pola dijadwalkan dengan selisih unsigned `millis()` yang menangani rollover.
  Panggil `update()` rutin dari satu loop/task pemilik. Update terlambat memilih
  fase saat ini tanpa memutar ulang transisi yang terlewat.
- Tidak ada `delay()` pola di driver. Pengiriman satu frame tetap sinkron:
  core versi ini memanggil `rmtWrite()` dengan waktu tunggu `RMT_WAIT_FOR_EVER`.
  Karena itu API ini bukan jaminan latensi real-time ketika peripheral bermasalah.
- Driver mengirim frame saat warna berubah; status yang tidak berubah tidak
  mengirim frame berulang. `begin()` mengirim OFF, berlaku setelah aplikasi mulai.
- API RGB core mengembalikan `void`; status cache tidak membuktikan keberhasilan
  RMT atau kondisi LED. Jika LED tidak merespons, periksa log core dan board fisik.
- `PIN_RGB_LED` dipastikan **48** pada compile time; tidak memakai alias
  `LED_BUILTIN` yang dapat berupa nomor virtual pada core.
- Revisi board berbeda bisa memakai GPIO lain. Kontrak proyek tetap GPIO 48;
  jika warna tidak muncul, pastikan board yang dimiliki cocok dengan gambar
  dan skematik sebelum mengubah kontrak pin.

## Uji perangkat

1. Reset dan pastikan LED OFF.
2. Kirim `t`: pastikan urutan merah–hijau–biru, lalu mati. Ini memeriksa urutan warna.
3. Kirim `0`, `1`, `2`: pastikan hijau, kuning, merah tetap.
4. Kirim `3`: pastikan kuning–merah bergantian.
5. Kirim `4`: pastikan merah berkedip cepat.
6. Kirim `s`: pastikan mati; kirim `i` untuk memeriksa mode OFF dan state null.

## Referensi

Pin dan kode diagnosis: dokumentasi master dan skematik proyek.
API diverifikasi pada header/source core terpasang versi 3.3.12. Referensi resmi:
[API RGB Arduino-ESP32](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-rgb-led.h),
[implementasi RGB/RMT](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-rgb-led.c),
dan [ESP32-S3-DevKitC-1 v1.0: RGB GPIO 48](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).
