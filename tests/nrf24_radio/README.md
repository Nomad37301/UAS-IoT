# Tes host transport nRF24

Dari akar proyek:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/nrf24_radio/fakes -I firmware/node_b/08_nrf24_radio tests/nrf24_radio/test_transport.cpp firmware/node_b/08_nrf24_radio/NodeBRadio.cpp -o .build/test_nrf24_transport.exe
& ./.build/test_nrf24_transport.exe
g++ -std=c++11 -Wall -Wextra -Werror -I tests/nrf24_radio/fakes -I firmware/node_b/08_nrf24_radio tests/nrf24_radio/test_packet.cpp firmware/node_b/08_nrf24_radio/NodeBRadio.cpp -o .build/test_nrf24_packet.exe
& ./.build/test_nrf24_packet.exe
```

Tes driver transport asli dengan RF24 simulasi: konfigurasi kontrak radio,
inisialisasi gagal, batas ukuran, status TX lama, BUSY, ACK, MAX_RETRIES,
RX dipertahankan saat TX, pemulihan FIFO, timeout, rollover, SPI 0xFF dan
inisialisasi ulang. Ukuran fixture 16 byte menguji transport generik.

Tes paket menguji layout packed 32 byte dan offset, byte golden little-endian,
nilai metrik belum tersedia, tujuh reserved byte, validasi perintah, pengulangan
seq terakhir, rollover seq dan pengiriman/penerimaan 32 byte melalui driver asli.
Tidak menguji PHY, daya RF, jangkauan, aktuator atau radio Node C fisik.
