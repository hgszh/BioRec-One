# BioRec-One

[中文](./README_ZH.md)

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/6.jpg"  width="600" />
</p>

A single-channel biopotential recorder designed for educational demonstration. The analog front end is built entirely from discrete components, and recorded data can be exported in CSV and BDF formats.

## Hardware Features

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/21.jpg"  width="600" />
</p>

- Analog front end built from discrete components — no monolithic ASIC, making it well-suited for teaching and demonstration
- Equivalent input noise density of 18.8 nV/√Hz; 266 nV RMS over a 200 Hz bandwidth
- Bandwidth 0.05–200 Hz; high-pass cutoff switchable between 0.05 Hz and 1.5 Hz; multiple digital filter modes available
- Isolated power supply for common-mode interference rejection
- FPGA-based FIR/IIR filtering and Pan-Tompkins heart rate detection

## Photos

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/84.jpg"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/82.jpg"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/83.jpg"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/86.gif"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/59.gif"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/79.jpg"  width="500" />
<img src="https://github.com/chirpyjay/BioRec-One/wiki/79_1.jpg"  width="500" />
</p>

## Repository Structure
```
├── hardware/           # EasyEDA Pro projects, schematics, Gerber files
│   ├── afe/            # Analog front-end module
│   ├── power-iso/      # ±5 V isolated power supply module
│   └── mainboard/      # Main board
├── firmware/           # FPGA (Gowin IDE) and MCU (ESP-IDF) projects, with flashing guide
└── examples/           # Sample CSV/BDF files
```

## Build Guide

**Main components:** main board, AFE module, isolated power supply module (3 PCBs total); display module; [Tang Nano 9K development board](https://wiki.sipeed.com/hardware/zh/tang/Tang-Nano-9K/Nano-9K.html) .

1. Go to the `hardware/` directory, order PCBs for each of the three subdirectories, and purchase and solder components according to the EasyEDA Pro projects.

2. Go to the `firmware/` directory and flash the FPGA and MCU firmware following the flashing guide.

3. Fabricate electrodes: wet electrodes are recommended for better contact impedance. See [Section 3.3.3 — Practical Electrodes](https://github.com/chirpyjay/BioRec-One/wiki/03-%E5%BF%83%E7%94%B5%E6%B5%8B%E9%87%8F%E5%8E%9F%E7%90%86#333%E5%AE%9E%E9%99%85%E7%94%B5%E6%9E%81%E5%8F%8A%E7%AD%89%E6%95%88%E6%A8%A1%E5%9E%8B) for details.

   <p align="center">
   <img src="https://github.com/chirpyjay/BioRec-One/wiki/85.jpg"  width="300" />
   </p>

4. Display module: ST7789V2, 2-inch, 240×320. Verify the pin assignment before purchasing.

5. Assembly: a brass standoff can be soldered at one corner of the display for support; four brass standoffs soldered into the main board mounting holes serve as feet.

6. Once assembly is complete, refer to [07 — User Guide](https://github.com/chirpyjay/BioRec-One/wiki/07-%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E).

------

## Documentation (Chinese)

**ECG Measurement Fundamentals / DIY Single-Channel Biopotential Recorder**

- [01 — Background](https://github.com/chirpyjay/BioRec-One/wiki/01-%E5%89%8D%E6%83%85%E5%9B%9E%E9%A1%BE)
- [02 — Goals and Motivation](https://github.com/chirpyjay/BioRec-One/wiki/02-%E7%9B%AE%E6%A0%87%E4%B8%8E%E6%84%8F%E4%B9%89)
- [03 — ECG Measurement Principles](https://github.com/chirpyjay/BioRec-One/wiki/03-%E5%BF%83%E7%94%B5%E6%B5%8B%E9%87%8F%E5%8E%9F%E7%90%86)
- [04 — System Architecture](https://github.com/chirpyjay/BioRec-One/wiki/04-%E6%95%B4%E4%BD%93%E6%9E%B6%E6%9E%84)
- [05 — Hardware Design](https://github.com/chirpyjay/BioRec-One/wiki/05-%E7%A1%AC%E4%BB%B6%E8%AE%BE%E8%AE%A1)
- [06 — FPGA Implementation](https://github.com/chirpyjay/BioRec-One/wiki/06-FPGA%E9%83%A8%E5%88%86%E5%AE%9E%E7%8E%B0)
- [07 — User Guide](https://github.com/chirpyjay/BioRec-One/wiki/07-%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E)
- [08 — Conclusion and Future Work](https://github.com/chirpyjay/BioRec-One/wiki/08-%E6%80%BB%E7%BB%93%E4%B8%8E%E5%B1%95%E6%9C%9B)

------

## License

This project is licensed under a multi-license model to cover hardware, software, and documentation.

### Hardware
**All hardware design files are released under the [CERN-OHL-P v2](https://ohwr.org/cern_ohl_p_v2.txt) license.**

Copyright (c) 2026 Zihang Shi (ChirpyJay)

This source describes Open Hardware and is licensed under the CERN-OHL-P v2.
You may redistribute and modify this documentation and make products using it under the terms of the CERN-OHL-P v2 (https://cern.ch/cern-ohl). 

### Software & Firmware
**All software and firmware code are released under the [MIT License](http://opensource.org/licenses/MIT).**

Copyright (c) 2026 Zihang Shi (ChirpyJay)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

### Documentation
**All documentation and Wiki content are released under [Creative Commons Attribution 4.0 International (CC BY 4.0)](http://creativecommons.org/licenses/by/4.0/).**

[![CC-BY-4.0](https://i.creativecommons.org/l/by/4.0/88x31.png)](http://creativecommons.org/licenses/by/4.0/)

You are free to Share and Adapt the material for any purpose, even commercially, provided that you give appropriate attribution (give credit, provide a link to the license, and indicate if changes were made).

## Disclaimer

This project is intended for educational and demonstration purposes only. **It does not constitute medical advice and must not be used for clinical diagnosis or any medical purpose.**

This device has not been certified as a medical device. The author accepts no liability for personal injury, property damage, or other safety incidents arising from reproducing, using, or modifying this project. Ensure you have the necessary electronics and safety knowledge before attempting to build this device.
