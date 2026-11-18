# Bagian 09 — LittleFS blackbox logger Node B

Sketch mandiri untuk log lokal ESP32-S3 saat jaringan tidak tersedia. Tidak
memerlukan modul tambahan atau library eksternal: `LittleFS` dan `FS` tersedia
dalam paket Arduino-ESP32. Integrasi diagnosis/sensor/protokol dilakukan nanti.

Dokumen master hanya menyebut LittleFS blackbox. Format JSON Lines, batas file,
rotasi dan interval di bawah merupakan pilihan implementasi bagian ini.

## Penyimpanan

| File | Fungsi | Batas data |
| --- | --- | --- |
| `/aether-current.jsonl` | Log yang sedang ditulis | 32768 byte |
| `/aether-previous.jsonl` | Generasi sebelum current | 32768 byte |

Jika record baru melewati batas, previous lama dihapus, current di-rename menjadi
previous, lalu record baru ditulis ke current. Total data kedua file maksimal
64 KiB untuk file yang dikelola driver; metadata/alokasi LittleFS memakai ruang
tambahan. Satu record tidak dipotong untuk memenuhi batas.

Mount memakai **`LittleFS.begin(false, "/littlefs", 4, "spiffs")`**: kegagalan
mount tidak menghapus data otomatis. Label partisi `spiffs` adalah label yang
dipakai wrapper LittleFS core, bukan perintah memakai format SPIFFS.

## Board, partisi dan upload

1. Buka `09_littlefs_blackbox.ino`, pertahankan semua `.ino`, `.h`, `.cpp` bersama.
2. Pilih **ESP32S3 Dev Module**, paket ESP32 yang diuji **3.3.12**.
3. Gunakan skema partisi yang menyediakan partisi data berlabel **spiffs** dan
   ruang minimal **128 KiB**. Sesuaikan flash/PSRAM dengan varian fisik board.
   Kompilasi proyek memakai tabel default 4 MB: partisi spiffs berukuran 0x160000.
4. Gunakan konektor **UART**, COM yang sesuai, **USB CDC On Boot: Disabled**.
5. Upload dan buka Serial Monitor **115200 baud**.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=default --warnings all firmware/node_b/09_littlefs_blackbox
```

Validasi komputer proyek: tes driver host dan parser JSON Python **PASS**,
kompilasi ESP32-S3 **PASS** (flash 317.261 byte, RAM global 21.832 byte).
Upload, persistensi flash fisik dan uji mati daya belum dilakukan.

Upload sketch mengganti program sebelumnya. Penghapusan seluruh flash, perubahan
layout partisi atau upload filesystem image dapat menghapus/menimpa log; untuk
menguji persistensi gunakan reset normal dengan layout partisi tetap.

## Perintah Serial Monitor

**Kirim satu perintah per baris**, pilih Newline atau Both NL & CR. Berbeda dari
sketch satu karakter sebelumnya, perintah diproses setelah Enter/newline.

| Perintah | Hasil |
| --- | --- |
| `w` | Tulis satu `TEST_EVENT`. |
| `a` | Aktif/nonaktifkan heartbeat uji tiap 10 detik; awalnya nonaktif. |
| `d` | Dump current ke serial bertahap. |
| `b` | Dump previous ke serial bertahap. |
| `x` | Batalkan dump. |
| `i` | Status mount, ukuran, session dan sequence. |
| `m` | Remount tanpa format, lalu mulai session logger baru. |
| `?` | Bantuan. |
| `FORMAT` | **Hapus seluruh isi partisi LittleFS**, lalu mount dan mulai session baru. |

Pada flash yang belum mempunyai filesystem LittleFS, MOUNT_FAILED adalah hasil
yang mungkin. Jika memang ingin menginisialisasi filesystem dengan menghapus
isinya, kirim tepat **`FORMAT`**. Tidak ada format otomatis saat gagal mount;
huruf `f`, awalan FORMAT atau baris terlalu panjang tidak memicu format.
Perintah FORMAT hanya disediakan dalam kode untuk pengguna; tidak dijalankan
ke perangkat pada pengerjaan ini.

Dump mengirim maksimal 64 byte per loop dengan memeriksa ruang TX serial.
Penulisan heartbeat ditunda selama dump; perintah lain diabaikan kecuali `x`,
agar stream/file tidak berubah selama dibaca. Ada penanda DUMP_BEGIN/DUMP_END
di luar isi file. Copy record JSON yang memiliki field `session` untuk analisis.
File previous mungkin belum ada sebelum rotasi pertama: LOG_NOT_FOUND normal.

## Format record dan API

Setiap record lengkap berakhir newline:

```json
{"system":"AETHER","node":"nodeB","session":"00001234","seq":0,"uptime_ms":42,"event":"TEST_EVENT","message":"Manual blackbox test"}
```

- Session 32 bit dibuat dari `esp_random()` saat logger diinisialisasi. Ini
  penanda sesi diagnostik dengan kemungkinan tabrakan, bukan ID unik terjamin.
- Seq uint32 mulai nol per session. Remount/reset menghasilkan session baru.
- `uptime_ms` adalah `millis()` lokal, bukan waktu Unix. Counter berulang sekitar
  49,7 hari; urutan record dibantu seq. Sinkronisasi jam belum diintegrasikan.
- Event wajib 1–24 karakter **A–Z, 0–9 atau underscore**.
- Message maksimal **96 byte**, tidak termasuk null terminator. Caller memasok
  teks UTF-8 yang valid; driver meng-escape quote, backslash dan karakter kontrol.
- Baris dibangun dalam buffer tetap 768 byte sebelum menyentuh file.

Contoh integrasi nanti, setelah filesystem mounted dan `begin(sessionId)` sukses:

```cpp
auto error = logger.append(millis(), "SERVICE_DOWN", "HTTP=503; MODBUS=OK");
```

Program mencatat SESSION_START saat mount sukses. Heartbeat hanya data uji;
bagian ini belum mencatat metrik sensor atau insiden diagnosis secara otomatis.

## IO, pemulihan dan batasan

- `begin()` memeriksa file tanpa mengubahnya. File milik logger yang oversized
  atau berupa direktori ditolak; tidak dihapus otomatis.
- Jika current tidak berakhir newline, append berikutnya merotasi current ke
  previous sebelum menulis. Ekor terputus dipertahankan di previous dan tidak
  disambung dengan record baru. Previous yang lebih lama diganti saat rotasi.
- Ini hanya pemeriksaan newline terakhir, bukan pemeriksaan JSON/checksum semua
  record. Reader harus mengabaikan baris terakhir yang tidak lengkap/tidak valid.
- Record ditulis dengan satu pemanggilan write, lalu flush dan close. Partial
  write/open/rotasi gagal menghentikan logger sampai `m` atau inisialisasi ulang.
- `flush()`/`close()` API core tidak menyediakan status error kepada driver;
  hasil OK tidak menjamin daya tahan terhadap semua skenario mati daya.
- Remove previous dan rename current adalah langkah terpisah. Rotasi dua file
  bukan transaksi atomik gabungan; power loss dapat mengurangi generasi tersimpan.
- IO filesystem bersifat **sinkron** dan dapat menunda loop, termasuk garbage
  collection. Ini bukan logger real-time; saat integrasi perlu menjadwalkan IO
  agar tidak menghambat penanganan relay/radio.
- Driver memakai buffer tetap; wrapper FS sendiri dapat melakukan alokasi
  internal. Semua API dipakai dari satu loop tanpa writer lain pada dua file ini.
- Heartbeat uji awalnya OFF. Untuk pemakaian akhir, utamakan perubahan state/
  insiden atau interval metrik yang wajar agar frekuensi tulis flash terkendali.

## Uji perangkat

1. Jalankan dan periksa `i`. Jika perlu inisialisasi filesystem kosong, gunakan
   FORMAT dengan memahami bahwa seluruh isi partisi akan hilang.
2. Kirim `w` beberapa kali lalu `d`; periksa JSON dan seq yang bertambah.
3. Reset normal tanpa erase flash. Dump kembali: catatan lama harus tetap ada,
   disusul SESSION_START dengan session baru.
4. Aktifkan heartbeat dengan `a`, cek pertambahan tiap 10 detik, lalu nonaktifkan.
5. Rotasi diuji deterministik melalui tes host. Saat uji flash fisik, isi sampai
   batas dan cek current/previous tidak melampaui 32768 byte.
6. Uji pemutusan daya hanya pada board uji; setelah menyala kembali, pastikan
   record baru tidak tersambung ke ekor record yang tidak lengkap.

Referensi: LittleFS blackbox pada dokumen master,
[wrapper LittleFS Arduino-ESP32](https://github.com/espressif/arduino-esp32/blob/master/libraries/LittleFS/src/LittleFS.h),
dan [proyek littlefs](https://github.com/littlefs-project/littlefs).
