# Pengujian host driver GY-906

Jalankan dari akar proyek dengan g++ (C++11 atau lebih baru):

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/gy906/fakes -I firmware/node_b/01_gy906 tests/gy906/test_mlx90614.cpp firmware/node_b/01_gy906/Mlx90614.cpp -o .build/test_gy906.exe
& ./.build/test_gy906.exe
```

Tes menjalankan driver asli terhadap bus simulasi: urutan register dan repeated
START, vektor PEC dari datasheet, konversi suhu positif/negatif, checksum rusak,
flag error sensor, short read, tidak ada respons, NACK, bus error, timeout,
kegagalan buffer TX, pembatalan data lama, dan pemulihan sampel berikutnya.
Tes ini tidak membuktikan kondisi wiring atau respons sensor fisik.
