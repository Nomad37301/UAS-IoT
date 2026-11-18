# Tes host active buzzer

Jalankan dari akar proyek dengan g++:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/active_buzzer/fakes -I firmware/node_b/03_active_buzzer tests/active_buzzer/test_active_buzzer.cpp firmware/node_b/03_active_buzzer/ActiveBuzzer.cpp -o .build/test_active_buzzer.exe
& ./.build/test_active_buzzer.exe
```

Driver asli diuji dengan GPIO simulasi dan waktu eksplisit: diam setelah begin,
pola Test/Warning/Critical pada batas waktunya, tidak mengulang pola yang sama,
mute langsung dan latch, unmute, test kedaluwarsa saat muted, tidak ada pulsa
sementara saat mute, update terlambat, pengurangan penulisan GPIO berulang,
overflow counter waktu dan pola tidak dikenal. Tes ini tidak mengukur suara,
arus buzzer, kondisi pin selama reset atau ketepatan timing pada board fisik.
