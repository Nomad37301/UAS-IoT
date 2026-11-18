# Tes host blackbox logger

Dari akar proyek:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/littlefs_blackbox/fakes -I firmware/node_b/09_littlefs_blackbox tests/littlefs_blackbox/test_blackbox.cpp firmware/node_b/09_littlefs_blackbox/BlackboxLogger.cpp -o .build/test_blackbox.exe
& ./.build/test_blackbox.exe
python tests/littlefs_blackbox/verify_json.py
```

Driver asli diuji pada FS simulasi: escaping JSON termasuk pesan terburuk,
validasi panjang/event, batas file tepat, dua rotasi, preservasi isi saat begin,
ekor terputus, partial write, recovery, kegagalan open/read/seek/remove/rename,
file oversized dan penolakan tulis setelah error IO. Output JSON juga diverifikasi
parser Python independen. Tidak membuktikan perilaku flash, persistensi fisik,
timing IO atau ketahanan mati daya pada perangkat.
