# Tes host relay recovery

Jalankan dari akar proyek dengan g++:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/recovery_relay/fakes -I firmware/node_b/06_recovery_relay tests/recovery_relay/test_recovery_relay.cpp firmware/node_b/06_recovery_relay/RecoveryRelay.cpp -o .build/test_recovery_relay.exe
& ./.build/test_recovery_relay.exe
```

Tes menjalankan driver asli terhadap GPIO simulasi: kondisi awal OFF, polaritas,
batas pulsa 499/500 ms, BUSY tanpa memperpanjang pulsa, batas cooldown 30 detik,
cancel, cancel berulang, pelepasan terlambat, tidak ada antrean retry, serta
overflow counter saat pulsa dan cooldown. Tidak menguji kontak mekanis atau
keberhasilan recovery Raspberry Pi.
