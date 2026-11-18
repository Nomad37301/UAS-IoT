# Bagian 04 — Tombol BOOT untuk mute Node B

Sketch mandiri untuk tombol **BOOT GPIO 0**, `INPUT_PULLUP`, aktif LOW.
Tombol memakai debounce polling tanpa interrupt. Contoh ini memakai driver
buzzer GPIO 15 dari bagian 03 untuk membuktikan fungsi mute.

`ActiveBuzzer.h/.cpp` di folder ini adalah salinan tanpa perubahan dari bagian 03
agar sketch dapat dibuka dan di-upload sendiri. Driver tombol berada pada
`DebouncedButton.h/.cpp`. Sketch terdahulu tetap tersedia; upload bagian ini
mengganti program yang berjalan di board. Penyatuan driver dilakukan saat integrasi.

## Wiring dan boot

- Gunakan tombol BOOT bawaan board; tidak perlu memasang tombol tambahan.
- Tombol menghubungkan GPIO 0 ke GND ketika ditekan. Jangan mengatur GPIO 0 sebagai OUTPUT.
- Untuk uji bunyi, gunakan wiring buzzer GPIO 15 dari bagian 03 dan periksa
  kecocokan rating listrik komponen sebagaimana panduan bagian tersebut.
- **Lepas BOOT saat reset/power-on untuk menjalankan aplikasi.** GPIO 0 yang LOW
  saat reset memilih serial bootloader. Driver aplikasi tidak bisa mengubah
  keputusan boot ROM ini. Jika masuk bootloader, lepas BOOT dan tekan RESET.

## Upload

1. Buka `04_boot_mute.ino`; biarkan semua `.ino`, `.h`, `.cpp` bersama dalam folder ini.
2. Pilih **ESP32S3 Dev Module**, paket ESP32 yang diuji **3.3.12**.
3. Gunakan konektor **UART**, port COM yang sesuai dan **USB CDC On Boot: Disabled**.
   Sesuaikan flash/PSRAM dengan board fisik.
4. Upload dan buka Serial Monitor **115200 baud**. Program mulai dengan buzzer OFF.

Alternatif kompilasi dari akar proyek:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/04_boot_mute
```

Validasi komputer proyek: tes driver tombol+buzzer host **PASS**, kompilasi
ESP32-S3 **PASS** (flash 303.642 byte, RAM global 22.160 byte).
Upload dan pengujian tombol/bunyi fisik belum dilakukan.

## Perilaku tombol

- Debounce **30 ms** pada penekanan dan pelepasan; durasi ini pilihan implementasi.
- Setelah mulai, input harus terlihat lepas/HIGH stabil selama 30 ms sebelum
  driver menerima penekanan pertama. Input awal LOW tidak memunculkan perintah mute.
- Penekanan LOW yang stabil menghasilkan satu event `BUTTON_MUTE` dan menambah
  `press_count` satu kali. Menahan tombol tidak mengulang event.
- Menekan lagi saat sudah muted tetap mute. Tombol **tidak** melakukan toggle.
- Mute mempertahankan pola alarm. Pilih `u` di Serial Monitor untuk unmute.
  Mute bertahan melewati perubahan pola dan `s`; reset board menghapus mute.
- Perintah serial `u` adalah override manual, termasuk bila tombol masih ditahan.
  Lepas lalu tekan kembali agar muncul event mute baru.
- Polling memakai selisih unsigned `millis()`, termasuk saat overflow. Panggil
  `update()` rutin dari satu loop; pulsa yang terjadi sepenuhnya di antara dua
  polling tidak dapat dideteksi. Waktu debounce adalah waktu stabil yang diamati.

## Perintah Serial Monitor

Kirim satu huruf kecil; newline CR/LF diabaikan.

| Perintah | Hasil |
| --- | --- |
| `w` / `c` | Mulai alarm WARNING / CRITICAL dari bagian 03. |
| `t` | Bunyi uji satu kali 100 ms. |
| `s` | Hentikan pola. |
| `m` / `u` | Mute / unmute melalui serial. |
| `i` / `?` | Status / bantuan. |

Contoh event (angka ilustrasi):

```json
{"system":"AETHER","node":"nodeB","component":"BOOT_MUTE","uptime_ms":3000,"event":"BUTTON_MUTE","button_pressed":true,"button_armed":false,"press_count":1,"pattern":"CRITICAL","muted":true,"output_high":false}
```

`button_pressed` menyatakan level tekan yang sudah stabil, `button_armed`
menyatakan driver siap menerima penekanan baru, dan `output_high` adalah output
yang diperintahkan ke buzzer, bukan pengukuran bunyi. Status juga dicetak per detik.

## Uji perangkat

1. Reset tanpa menahan BOOT. Pastikan buzzer diam, hitungan nol; setelah 30 ms
   kondisi lepas stabil, `button_armed=true`.
2. Kirim `c` untuk alarm, tekan BOOT: buzzer diam, `muted=true`, hitungan menjadi 1.
3. Tahan BOOT beberapa detik: hitungan tetap 1. Lepas, tunggu lebih dari 30 ms,
   tekan kembali: hitungan menjadi 2 dan buzzer tetap muted.
4. Lepas tombol dan kirim `u`: alarm berbunyi lagi. Tekan BOOT untuk mute kembali.
5. Kirim `s`: pastikan diam; kirim `u` ketika pola OFF, pastikan tidak ada bunyi.

Program belum menggabungkan sensor suhu/tegangan, RGB, relay, MQTT atau radio.

Referensi: pin/polaritas pada dokumen master proyek,
[API GPIO Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html),
dan [pemilihan boot mode ESP32-S3](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).
