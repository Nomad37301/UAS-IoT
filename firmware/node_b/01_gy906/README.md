# Bagian 01 — GY-906 / MLX90614 pada Node B

Program mandiri untuk memeriksa satu komponen sebelum integrasi firmware AETHER.
Target: ESP32-S3 sesuai dokumentasi master. Bahasa: C++ dengan Arduino ESP32.
Driver mengakses register SMBus langsung melalui `Wire`; tidak memerlukan library
sensor tambahan. Semua file `.ino`, `.h`, dan `.cpp` dalam folder ini harus tetap bersama.

## Wiring

| GY-906 | Node B ESP32-S3 |
| --- | --- |
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

Alamat 7-bit `0x5A`, clock I2C **100 kHz**. Ikuti label pin modul.
Pastikan pull-up SDA/SCL pada modul menuju 3,3 V dan modul mendukung suplai 3,3 V
seperti kontrak hardware proyek. Kode ini mengasumsikan sensor masih dalam mode
SMBus dan memakai alamat bawaan; tidak mengubah konfigurasi EEPROM.

## Upload dengan Arduino IDE

1. Buka `01_gy906.ino` dari folder ini.
2. Pilih board **ESP32S3 Dev Module** dari paket **esp32 by Espressif Systems**.
   Pengujian kompilasi proyek memakai versi paket yang tersedia, **3.3.12**.
3. Gunakan port **UART** pada board, sesuai gambar referensi proyek, lalu pilih
   port COM yang muncul. Untuk koneksi USB-UART ini, gunakan **USB CDC On Boot: Disabled**.
   Biarkan ukuran flash/PSRAM mengikuti varian board fisik yang dimiliki.
4. Upload. Bila board tidak masuk mode upload, tahan BOOT, tekan-lepas RESET,
   lalu lepas BOOT ketika proses koneksi upload berjalan.
5. Buka Serial Monitor **115200 baud**, tekan RESET untuk melihat pesan awal.

Alternatif kompilasi lewat Arduino CLI dari akar proyek, setelah paket ESP32 tersedia:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/01_gy906
```

Validasi di komputer ini: kompilasi ESP32-S3 dan tes driver host lulus.
Upload serta pengujian pada sensor fisik belum dilakukan.

## Perilaku

- Menunggu 1 detik setelah boot, lalu mengambil sampel setiap 1 detik.
- Membaca ambient dari register `0x06`, object dari `0x07` memakai repeated START.
- Memvalidasi PEC CRC-8 (`0x07`, initial `0x00`) dan error flag bit 15.
- Konversi: `suhu_c = raw * 0.02 - 273.15`.
- Satu pasangan dinyatakan valid hanya bila kedua register berhasil dibaca.
- Transaksi Wire diberi timeout 50 ms; tidak ada loop retry tanpa batas.
- Ketika pembacaan gagal, sampel berikutnya tetap dicoba tiap 1 detik.
- Output suhu gagal menjadi `null`; data lama tidak dipakai sebagai data baru.

Contoh bentuk output (angka hanya ilustrasi):

```json
{"system":"AETHER","node":"nodeB","component":"GY-906","seq":1,"uptime_ms":1000,"valid":true,"ir_obj_c":37.81,"ir_amb_c":27.13,"error":"OK"}
```

```json
{"system":"AETHER","node":"nodeB","component":"GY-906","seq":2,"uptime_ms":2000,"valid":false,"ir_obj_c":null,"ir_amb_c":null,"error":"I2C_SHORT_READ_OR_NO_RESPONSE"}
```

`ir_obj_c` mengukur permukaan yang dilihat sensor; `ir_amb_c` adalah temperatur
sensor, sebagai perkiraan ambient. Suhu inti CPU nantinya berasal dari Modbus.

## Pengujian di perangkat

1. Pastikan keluaran `valid:true` dengan dua nilai suhu yang masuk akal.
2. Arahkan telapak tangan ke bidang pandang sensor tanpa menyentuhnya; amati
   perubahan `ir_obj_c`. Kecepatan respons dipengaruhi filter internal sensor.
3. Dengan daya dimatikan, lepas sensor, lalu nyalakan board: keluaran harus
   `valid:false`, kedua suhu `null`, dan log tetap berjalan.
4. Matikan daya, sambungkan kembali, nyalakan dan pastikan keluaran valid kembali.

| Error | Makna / pemeriksaan |
| --- | --- |
| `I2C_INIT_FAILED` | Inisialisasi bus gagal; cek board dan penggunaan GPIO. |
| `I2C_NACK` | NACK pada tahap kirim, bila dilaporkan Wire. |
| `I2C_SHORT_READ_OR_NO_RESPONSE` | Kurang dari tiga byte; cek daya, kabel, alamat, pull-up. ESP32 dapat melaporkan NACK/timeout pada tahap baca sebagai nol byte. |
| `PEC_MISMATCH` | Checksum gagal; periksa noise, kabel dan suplai. |
| `SENSOR_ERROR_FLAG` | Sensor menandai hasil tidak valid. |
| `I2C_TIMEOUT` / `I2C_BUS_ERROR` | Kegagalan tahap kirim yang dilaporkan Wire. |
| `I2C_TX_BUFFER_ERROR` | Byte perintah gagal dimasukkan ke buffer Wire. |

## Referensi

Pin, alamat dan clock: `DOKUMENTASI_LENGKAP_HARDWARE_DAN_SOFTWARE.md` serta
`SKEMATIK_DAN_PINOUT.md` pada akar proyek.

Detail register, urutan SMBus, PEC dan konversi mengikuti
[datasheet resmi Melexis MLX90614](https://www.melexis.com/-/media/files/documents/datasheets/mlx90614-datasheet-melexis.pdf),
bagian 4.1.4 dan 4.1.8. Vektor PEC rujukan pada gambar 6:
alamat `0x5A`, register `0x07`, data `0x3AD2`, PEC `0x30`.

