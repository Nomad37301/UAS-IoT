# Bagian 06 — Relay recovery Node B

Sketch mandiri untuk **GPIO 16, modul relay aktif HIGH**, sesuai jawaban pengguna.
HIGH memerintahkan relay ON; LOW memerintahkannya OFF. Perintah recovery berupa
satu pulsa nominal **500 ms**, diikuti cooldown **30 detik setelah dilepas**.
Durasi dan cooldown merupakan pilihan implementasi, bukan nilai yang ditetapkan
master. Pulsa 500 ms lebih panjang dari debounce input daemon Pi 100 ms pada master.

## Wiring sesuai proyek

| Modul relay | Sambungan |
| --- | --- |
| VCC | 5 V Node B |
| GND | Common GND |
| IN | GPIO 16 ESP32-S3 |
| COM | Common GND / Raspberry Pi pin fisik 6 |
| NO | Raspberry Pi pin fisik 11 / BCM GPIO 17 |
| NC | Kosongkan |

Pastikan input modul dapat dipicu logika 3,3 V dari ESP32-S3 meskipun VCC modul
5 V. GPIO mengendalikan **IN modul**, bukan kumparan relay secara langsung.
Kontak NO/COM berfungsi sebagai dry contact ke GND, tidak menyalurkan 5 V ke pin 11.

Dalam skema proyek, pin 11 adalah **input GPIO ke daemon recovery di Raspberry Pi**.
Menutup kontak meminta daemon melakukan recovery. GPIO ini bukan pin reset
hardware; jika host/kernel macet total sehingga daemon tidak berjalan, kontak
ini sendiri tidak menjamin reboot. Daemon Node A akan dibuat pada bagian tersendiri.

## Upload

1. Buka `06_recovery_relay.ino`, pertahankan seluruh file `.ino`, `.h`, `.cpp` bersama.
2. Pilih **ESP32S3 Dev Module**, paket ESP32 yang diuji **3.3.12**.
3. Gunakan konektor **UART**, port COM yang sesuai, **USB CDC On Boot: Disabled**,
   serta ukuran flash/PSRAM sesuai board fisik.
4. Upload dan buka Serial Monitor **115200 baud**. Output dimulai LOW/OFF;
   tidak ada pulsa uji/recovery otomatis saat program mulai.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/06_recovery_relay
```

Validasi komputer proyek: tes driver host **PASS**, kompilasi ESP32-S3 **PASS**
(flash 303.194 byte, RAM global 22.152 byte pada konfigurasi tersebut).
Upload, uji kontak fisik dan recovery Raspberry Pi belum dilakukan.

Upload mengganti program yang berjalan. Sketch bagian terdahulu tetap tersimpan;
bagian ini belum menggabungkan sensor, tombol, buzzer, RGB, MQTT, CoAP atau radio.

## Perintah Serial Monitor

Kirim satu huruf kecil; CR/LF dan whitespace diabaikan.

| Perintah | Hasil |
| --- | --- |
| `r` | Minta satu pulsa recovery. |
| `s` | Batalkan pulsa aktif: OFF segera, lalu cooldown. |
| `i` | Status JSON. |
| `?` | Bantuan. |

Hasil `r`: `STARTED` jika diterima, `BUSY` ketika pulsa masih aktif,
`COOLDOWN` selama jeda. Permintaan yang ditolak tidak memperpanjang pulsa,
tidak masuk antrean dan tidak dijalankan nanti. `s` ketika sudah OFF tidak
mengulang cooldown. Durasi tersedia di `RecoveryRelay.h`.

Contoh keluaran:

```json
{"system":"AETHER","node":"nodeB","component":"RECOVERY_RELAY","uptime_ms":1000,"event":"STARTED","active_level":"HIGH","relay_active":true,"cooldown_remaining_ms":0,"accepted_count":1}
```

Status juga dicetak tiap detik. `relay_active` mencerminkan perintah output
driver, bukan feedback kontak mekanis atau konfirmasi Raspberry Pi telah pulih.
`accepted_count` menghitung pulsa diterima, termasuk yang kemudian dibatalkan.
Cooldown nol saat pulsa ON; periksa `relay_active` sebelum menganggap siap.

## Cara kerja dan batasan

- `trigger()` memulai satu pulsa, `update()` melepas output setelah durasi tercapai.
  Pola memakai selisih unsigned `millis()`, termasuk saat overflow.
- Tidak ada `delay()` penantian pulsa/cooldown atau retry tak terbatas di driver.
  Semua metode dipakai dari satu loop pemilik; callback jaringan nanti perlu
  menyalurkan perintah ke loop tersebut.
- `update()` wajib dipanggil rutin. **500 ms adalah nominal:** jika loop berhenti
  atau terblokir, output dapat aktif lebih lama. Ini bukan pembatas pulsa hardware.
  Cooldown dimulai saat output benar-benar diperintahkan OFF oleh driver.
- Reset ESP32 menghapus cooldown RAM. Jaminan keadaan output hanya setelah
  `begin()`; keadaan saat bootloader/reset bergantung pada rangkaian fisik.
- API GPIO tidak memberi feedback mekanis; ketepatan kontak perlu diuji fisik.
- Driver mendukung kedua polaritas melalui parameter konstruktor eksplisit;
  sketch proyek memilih **ActiveHigh** sesuai konfirmasi pengguna.

## Uji perangkat

1. Uji awal dengan NO belum tersambung ke Raspberry Pi agar yang diamati hanya
   gerakan kontak. Matikan daya sebelum mengubah wiring.
2. Jalankan program: pastikan relay OFF. Kirim `r`: harus ON sekitar 500 ms,
   lalu OFF. Gunakan continuity tester pada NO/COM untuk memverifikasi kontak.
3. Kirim `rr`: hanya permintaan pertama diterima, kedua `BUSY`.
4. Kirim `r` lagi setelah OFF tetapi sebelum 30 detik: harus `COOLDOWN`, tanpa klik.
5. Tunggu cooldown selesai, lalu ulangi. Ketika pulsa ON, kirim `s`: harus OFF
   dan cooldown dimulai dari pembatalan, bukan dari jadwal selesai pulsa.
6. Sambungkan kembali NO ke pin 11 dalam kondisi daya mati. Uji recovery host
   dilakukan sesudah daemon Node A tersedia; jangan menganggap klik sebagai
   bukti server sudah pulih.

Referensi pin/kontak: dokumentasi master dan skematik proyek. API GPIO:
[dokumentasi resmi Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html).
