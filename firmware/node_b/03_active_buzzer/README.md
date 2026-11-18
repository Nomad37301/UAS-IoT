# Bagian 03 — Active buzzer pada Node B

Sketch pengujian mandiri untuk **active buzzer, aktif HIGH** pada GPIO 15
ESP32-S3, mengikuti kontrak proyek. Active buzzer menghasilkan nada sendiri;
driver hanya mengatur ON/OFF. Pilihan pola dan durasi di bawah adalah keputusan
implementasi untuk pengujian, belum merupakan pemetaan diagnosis sistem.

## Wiring

| Buzzer | Node B |
| --- | --- |
| Kaki + / panjang | GPIO 15 |
| Kaki - / pendek | GND |

Dokumen belum mencantumkan rating listrik buzzer. Wiring langsung ini mengasumsikan
buzzer cocok untuk tegangan GPIO 3,3 V dan arusnya berada dalam kemampuan GPIO.
Periksa rating komponen sebelum uji fisik; bila memerlukan arus lebih besar atau
suplai lain, gunakan rangkaian driver yang sesuai. Kode ini untuk active buzzer,
bukan modul aktif LOW atau buzzer pasif yang memerlukan PWM.

## Upload

1. Buka `03_active_buzzer.ino`. Semua file `.ino`, `.h`, `.cpp` harus tetap dalam folder ini.
2. Pilih **ESP32S3 Dev Module**, paket **esp32 by Espressif Systems**.
   Versi paket pada komputer proyek adalah **3.3.12**.
3. Gunakan konektor **UART** dan port COM board; pilih **USB CDC On Boot: Disabled**.
   Sesuaikan flash/PSRAM dengan varian board fisik.
4. Upload; buka Serial Monitor **115200 baud**.
5. Program mulai dengan pola OFF dan tidak membunyikan test otomatis.

Sketch ini mengganti program di board saat di-upload; bagian 01 dan 02 tetap
tersimpan sebagai sketch terpisah. Integrasi dilakukan pada tahap berikutnya.

Alternatif kompilasi dari akar proyek:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/03_active_buzzer
```

Validasi komputer proyek: tes driver host **PASS**, kompilasi ESP32-S3 **PASS**
(flash 303.054 byte, RAM global 22.144 byte pada konfigurasi tersebut).
Upload serta pengujian bunyi pada perangkat fisik belum dilakukan.

## Perintah Serial Monitor

Kirim **satu huruf kecil**. Pengaturan newline bebas; CR/LF diabaikan.

| Perintah | Hasil |
| --- | --- |
| `t` | Bunyi satu kali 100 ms, lalu otomatis OFF. |
| `w` | WARNING: ON 150 ms, OFF 850 ms, berulang. |
| `c` | CRITICAL: ON 200 ms, OFF 200 ms, berulang. |
| `s` | Hentikan pola, output segera LOW. |
| `m` | Mute: output segera LOW; pola alarm tetap tersimpan. |
| `u` | Unmute: mulai fase bunyi pola aktif dari awal. |
| `i` | Cetak status. |
| `?` | Cetak bantuan. |

Mute bertahan melewati perubahan pola dan perintah `s` sampai `u` atau reset board.
Test tetap kedaluwarsa setelah 100 ms walaupun muted; unmute setelah test berakhir
tidak memainkannya lagi. Mengirim pola yang sama saat masih aktif tidak mengulang
fase. Untuk memulai ulang pola yang sama, kirim `s`, lalu pilih pola.

Contoh status:

```json
{"system":"AETHER","node":"nodeB","component":"ACTIVE_BUZZER","uptime_ms":1000,"pattern":"WARNING","muted":false,"output_high":true}
```

Status dicetak setelah perintah dan tiap satu detik. `output_high` adalah status
output yang diperintahkan driver, bukan hasil pengukuran bunyi atau feedback GPIO.

## Cara kerja dan batasan

- `begin()` mengatur OUTPUT lalu LOW; kontrol software berlaku setelah inisialisasi,
  bukan jaminan keadaan pin sepanjang reset/bootloader.
- Pola memakai selisih `uint32_t` dari `millis()`, termasuk saat counter melimpah.
- `update()` harus dipanggil rutin. Jika loop terlambat, driver memilih fase saat
  ini; tidak memutar ulang beep yang terlewat. Durasi aktual dipengaruhi latensi loop.
- Tidak memakai `delay()` di driver, PWM, alokasi dinamis atau task tambahan.
  Sketch melakukan yield 1 ms dan membatasi pemrosesan serial per iterasi.
- `setPattern()`, `setMuted()`, dan `update()` digunakan dari satu loop/task.
  Untuk integrasi dari callback atau task lain, salurkan perintah ke loop pemilik.
- Tombol BOOT GPIO 0, diagnosis, relay, dan perintah MQTT/radio belum dihubungkan.
  API mute sudah tersedia untuk integrasi bagian tersebut nanti.

## Uji perangkat

1. Setelah reset, pastikan buzzer diam dan status OFF.
2. Kirim `t`: bunyi pendek satu kali, kemudian diam.
3. Kirim `w`, lalu `c`: bandingkan irama peringatan dan alarm cepat.
4. Saat `c`, kirim `m`: harus diam dengan pola CRITICAL masih tercatat.
5. Kirim `w` ketika muted: tetap diam; `u` membunyikan WARNING.
6. Kirim `s`: diam. Kirim `m`, `t`, tunggu satu detik, lalu `u`:
   test yang sudah berakhir tidak boleh berbunyi lagi.

Referensi pin dan polaritas: dokumentasi master serta skematik di akar proyek.
API GPIO mengikuti [dokumentasi resmi Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html).
