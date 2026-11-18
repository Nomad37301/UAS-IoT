# Panduan Rakit & Skematik Final: Sistem **AETHER**
## **AETHER — Autonomous External Telemetry & Hardware Emergency Recovery**
**(Lantai 1: Node B `ESP32-S3` | Lantai 2: Node A `Raspberry Pi 5` | Eksternal: Node C `ESP32 30-Pin`)**

> **Referensi Gambar Visual & Dokumentasi Master:**
> - [SKEMATIK_VISUAL_NODE_AB.png](file:///c:/Users/luffy/Downloads/Proto_IoT/SKEMATIK_VISUAL_NODE_AB.png) — Skematik blok visual Lantai 1 & Lantai 2.
> - [DATASHEET_ANOTASI_NODE_AB.png](file:///c:/Users/luffy/Downloads/Proto_IoT/DATASHEET_ANOTASI_NODE_AB.png) — Datasheet asli `ESP32-S3` (diputar 180° sesuai posisi USB-C di atas) & `Raspberry Pi 5`.
> - [DOKUMENTASI_LENGKAP_HARDWARE_DAN_SOFTWARE.md](file:///c:/Users/luffy/Downloads/Proto_IoT/DOKUMENTASI_LENGKAP_HARDWARE_DAN_SOFTWARE.md) — Dokumentasi Master Kontrak Hardware & Software **AETHER**.

---

## 1. Tabel Urut Solderan `Node B` (Lantai 1 — `ESP32-S3` Port USB-C di ATAS)
Seluruh pin disolder pada **BARISAN PIN KANAN `ESP32-S3`** (urut dari `#01` paling atas dekat USB-C `UART` hingga `#22` paling bawah):

| Urutan dari Atas | Label Pin `ESP32-S3` | Disolder ke Komponen / Jalur | Catatan Hardware |
| :---: | :---: | :--- | :--- |
| **`#01`** | **`G` (`GND`)** | **Rel `GND` Utama Lantai 1** | Semua `GND`, Kaki `-` Elco, Baut `COM` Relay, & `Pin 6` Raspi 5 |
| **`#02`** | **`5V`** | **Pin `VCC` Modul Relay** | Power 5V kumparan Relay |
| **`#03`** | **`14` (`GPIO 14`)** | **Pin 3 (`CE`) `nRF24L01+ #1`** | Radio Chip Enable |
| **`#04`** | **`13` (`GPIO 13`)** | **Pin 7 (`MISO`) `nRF24L01+ #1`** | SPI MISO |
| **`#05`** | **`12` (`GPIO 12`)** | **Pin 5 (`SCK`) `nRF24L01+ #1`** | SPI Clock |
| **`#06`** | **`11` (`GPIO 11`)** | **Pin 6 (`MOSI`) `nRF24L01+ #1`** | SPI MOSI |
| **`#07`** | **`10` (`GPIO 10`)** | **Pin 4 (`CSN`) `nRF24L01+ #1`** | SPI Chip Select |
| **`#08`** | **`9` (`GPIO 9`)** | **Pin `SCL` pada `GY-906`** | I2C Clock |
| `#09` & `#10` | `46` & `3` | *(Kosongkan)* | Pin Strapping |
| **`#11`** | **`8` (`GPIO 8`)** | **Pin `SDA` pada `GY-906`** | I2C Data |
| **`#12`** | **`18` (`GPIO 18`)** | **[JUMPER 3] ke `Pin 8 (TXD)` Raspi 5** | UART1 `RX` S3 $\leftarrow$ `TX` Raspi |
| **`#13`** | **`17` (`GPIO 17`)** | **[JUMPER 4] ke `Pin 10 (RXD)` Raspi 5** | UART1 `TX` S3 $\rightarrow$ `RX` Raspi |
| **`#14`** | **`16` (`GPIO 16`)** | **Pin `IN` Modul Relay** | Pemicu *Hardware Emergency Recovery* |
| **`#15`** | **`15` (`GPIO 15`)** | **Kaki Panjang (`+`) Active Buzzer** | Kaki pendek (`-`) ke `Rel GND` |
| `#16`–`#18` | `7`, `6`, `5` | *(Kosongkan)* | Indikator di Node B pakai Onboard RGB LED (`GPIO 48`) |
| **`#19`** | **`4` (`GPIO 4`)** | **Pin `S` Sensor Tegangan 0–25V** | Pin `+` tengah sensor tegangan **dikosongkan**; Pin `-` ke `GND` |
| `#20` | `RST` | *(Kosongkan)* | - |
| **`#21` & `#22`** | **`3V3`** | **Rel `3.3V` Lantai 1** | Ke `VIN` GY-906, `Pin 2 (VCC)` nRF24L01+, & Kaki `+` Elco |

---

## 2. Daftar 5 Kabel Jumper Vertikal (`Lantai 1 Node B` $\updownarrow$ `Lantai 2 Raspi 5`)

| Nama Jumper | Dari Lantai 1 (Node B) | Colok ke `Raspberry Pi 5` (Lantai 2) | Fungsi |
| :--- | :--- | :--- | :--- |
| **`JUMPER 1`** | Baut Biru **`VCC`** Sensor Tegangan | **`Pin 2` (`5V`)** | Mengukur tegangan 5V Raspi 5 |
| **`JUMPER 2`** | Jalur **`Rel GND Utama`** Lantai 1 | **`Pin 6` (`GND`)** | *Common Ground* wajib |
| **`JUMPER 3`** | **`Pin #12` (`GPIO 18 / RX1`)** ESP32-S3 | **`Pin 8` (`GPIO 14 / TXD`)** | **UART / Modbus** (`TX` Raspi $\rightarrow$ `RX` S3) |
| **`JUMPER 4`** | **`Pin #13` (`GPIO 17 / TX1`)** ESP32-S3 | **`Pin 10` (`GPIO 15 / RXD`)** | **UART / Modbus** (`RX` Raspi $\leftarrow$ `TX` S3) |
| **`JUMPER 5`** | Baut Biru **`NO`** Modul Relay | **`Pin 11` (`GPIO 17`)** | **Hardware Emergency Recovery Trigger** |

---

## 3. Tabel Solderan `Node C` (Meja Admin — `ESP32 30-Pin`)

| Komponen di Node C | Pin di Komponen | Solder ke Pin `ESP32 30-Pin` | Catatan |
| :--- | :--- | :--- | :--- |
| **Layar `OLED 0.96"` (I2C)** | `VCC`, `GND` | **`3V3`**, **`GND`** | Cek label di modul OLED |
| | `SCL`, `SDA` | **`D22` (`GPIO 22`)**, **`D21` (`GPIO 21`)** | I2C Bus Node C |
| **`nRF24L01+ #2` (SPI)** | Pin 1 (`GND`), Pin 2 (`VCC`) | **`GND`**, **`3V3`** | + Elco 10uF (`-` ke GND, `+` ke 3V3) |
| | `CE`, `CSN`, `SCK`, `MOSI`, `MISO`| **`D4`**, **`D5`**, **`D18`**, **`D23`**, **`D19`**| VSPI Bus Node C |
| **3x LED Paralel (GPIO)** | Hijau, Kuning, Merah (`+`) | **`D25`**, **`D26`**, **`D27`** | Masing-masing kaki `-` $\rightarrow$ Resistor 220$\Omega$ $\rightarrow$ `GND` |
| **Push Button** *(Remote Recovery)*| Kaki 1, Kaki 2 | **`D33` (`GPIO 33`)**, **`GND`** | `INPUT_PULLUP` |
