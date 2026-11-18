# Tes host Modbus master

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/modbus_master/fakes -I firmware/node_b/07_modbus_master tests/modbus_master/test_modbus_master.cpp firmware/node_b/07_modbus_master/ModbusRtuMaster.cpp -o .build/test_modbus_master.exe
& ./.build/test_modbus_master.exe
```

Jalankan dari akar proyek. Driver asli diuji terhadap UART simulasi: request/CRC
yang diketahui nilainya, dekode lima register, CRC rusak, alamat/fungsi/count salah,
data di luar kontrak, frame pendek/berlebih, exception, echo FC06 salah, BUSY,
operasi tidak dikenal, timeout, tidak ada retry otomatis, byte terlambat sebelum
TX, respons terfragmentasi, data lama tidak dipakai, write parsial, kapasitas TX,
quiet gap dan overflow `micros()`. Tes tidak membuktikan timing byte fisik UART
atau fungsi slave Raspberry Pi.
