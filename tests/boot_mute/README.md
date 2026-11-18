# Tes host tombol BOOT/mute

Jalankan dari akar proyek dengan g++:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/boot_mute/fakes -I firmware/node_b/04_boot_mute tests/boot_mute/test_boot_mute.cpp firmware/node_b/04_boot_mute/DebouncedButton.cpp firmware/node_b/04_boot_mute/ActiveBuzzer.cpp -o .build/test_boot_mute.exe
& ./.build/test_boot_mute.exe
```

Pengujian menjalankan driver tombol dan buzzer asli dengan GPIO simulasi:
konfigurasi pull-up, penekanan dan pelepasan yang bouncing, batas 29/30 ms,
tahan lama, startup LOW, re-arm setelah lepas stabil, rollover waktu,
mute dari tombol ke buzzer, pola alarm dipertahankan, unmute manual dan mute
idempoten. Tes tidak membuktikan bunyi fisik atau perilaku ROM saat reset.
