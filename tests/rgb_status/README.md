# Tes host RGB status

Jalankan dari akar proyek dengan g++:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/rgb_status/fakes -I firmware/node_b/05_rgb_status tests/rgb_status/test_status_rgb.cpp firmware/node_b/05_rgb_status/StatusRgb.cpp -o .build/test_rgb_status.exe
& ./.build/test_rgb_status.exe
```

Driver asli dijalankan dengan pengirim frame RGB simulasi dan waktu eksplisit.
Kasus utama: kontrak kode state/GPIO48/GRB, pemetaan warna, batas transisi,
status sama tidak mengulang fase, test berakhir OFF, restart test, loop terlambat,
overflow waktu, state tidak dikenal dan intensitas. Tes tidak mengukur keluaran
RMT, warna cahaya fisik atau kecocokan varian board.
