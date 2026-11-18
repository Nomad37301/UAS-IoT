# Tes host sensor tegangan

Jalankan dari akar proyek dengan g++:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -I tests/voltage_adc/fakes -I firmware/node_b/02_voltage_adc tests/voltage_adc/test_voltage_sensor.cpp firmware/node_b/02_voltage_adc/VoltageSensor.cpp -o .build/test_voltage_adc.exe
& ./.build/test_voltage_adc.exe
```

Driver asli diuji terhadap fungsi ADC simulasi. Kasus: konfigurasi pin/resolusi/
attenuasi, rata-rata dengan pecahan mV, rasio pembagi, batas tepat 4,5 V,
tegangan rendah/nol, clipping di tengah window, batas rentang mV, kalibrasi,
konfigurasi tidak sah, hasil negatif, penghapusan data lama dan pemulihan.
Tes tidak membuktikan akurasi ADC, toleransi resistor, wiring atau kondisi suplai fisik.
