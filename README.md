# BioRec-One

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/6.jpg"  width="600" />
</p>

单通道电生理记录仪，面向教学演示和科研场景。离散元件搭建模拟前端，等效输入噪声约266nV，数据可导出为CSV和BDF格式。

## 硬件特性

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/21.jpg"  width="600" />
</p>

- 离散元件搭建模拟前端，不使用单片ASIC，适合教学演示
- 等效输入噪声密度18.8nV/√Hz，200Hz带宽下有效值266nV
- 带宽0.05–200Hz，高通截止频率可在0.05Hz / 1.5Hz间切换，支持多档数字滤波
- 隔离供电，抑制共模干扰
- FPGA实现FIR/IIR滤波与Pan-Tompkins心率检测

## 实物图片

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/84.jpg"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/82.jpg"  width="500" />
</p>

<p align="center">
<img src="https://github.com/chirpyjay/BioRec-One/wiki/83.jpg"  width="500" />
</p>

## 目录结构
```
├── hardware/           # 立创EDA Pro工程、原理图、Gerber
│   ├── afe/            # 模拟前端模块
│   ├── power-iso/      # ±5V隔离电源模块
│   └── mainboard/      # 主板
├── firmware/           # FPGA（高云云源）与 MCU（ESP-IDF）工程，含烧录指南
├── docs/               # 详细文档（施工中）
└── examples/           # 示例CSV/BDF文件
```

## 复刻说明

**主要组成**：主板、AFE模块、隔离电源模块（共3块PCB）；屏幕模块；Tangnano9K开发板。

1. 在 `hardware/` 目录找到3个子目录，分别下单PCB，并按照立创EDA工程购买元器件焊接。

2. 在 `firmware/` 目录按烧录指南分别烧录FPGA和MCU固件。

3. 制作电极，推荐湿电极以获得更好的接触阻抗。可参考 [3.3.3 实际电极](https://github.com/chirpyjay/BioRec-One/wiki/03-%E5%BF%83%E7%94%B5%E6%B5%8B%E9%87%8F%E5%8E%9F%E7%90%86#333%E5%AE%9E%E9%99%85%E7%94%B5%E6%9E%81%E5%8F%8A%E7%AD%89%E6%95%88%E6%A8%A1%E5%9E%8B) 中的介绍。

   <p align="center">
   <img src="https://github.com/chirpyjay/BioRec-One/wiki/85.jpg"  width="300" />
   </p>

4. 屏幕模块：ST7789V2，2寸，240×320，购买时注意核对引脚定义。

5. 装配：屏幕一角可焊铜柱支撑，主板安装孔可焊四个铜柱作脚垫。

------

## 文章

**心电测量基础 / DIY单道电生理记录仪**

- [01 前情回顾](https://github.com/chirpyjay/BioRec-One/wiki/01-%E5%89%8D%E6%83%85%E5%9B%9E%E9%A1%BE)
- [02 目标与意义](https://github.com/chirpyjay/BioRec-One/wiki/02-%E7%9B%AE%E6%A0%87%E4%B8%8E%E6%84%8F%E4%B9%89)
- [03 心电测量原理](https://github.com/chirpyjay/BioRec-One/wiki/03-%E5%BF%83%E7%94%B5%E6%B5%8B%E9%87%8F%E5%8E%9F%E7%90%86)
- [04 整体架构](https://github.com/chirpyjay/BioRec-One/wiki/04-%E6%95%B4%E4%BD%93%E6%9E%B6%E6%9E%84)
- [05 硬件设计](https://github.com/chirpyjay/BioRec-One/wiki/05-%E7%A1%AC%E4%BB%B6%E8%AE%BE%E8%AE%A1)
- [06 FPGA部分实现](https://github.com/chirpyjay/BioRec-One/wiki/06-FPGA%E9%83%A8%E5%88%86%E5%AE%9E%E7%8E%B0)
- [07 使用说明](https://github.com/chirpyjay/BioRec-One/wiki/07-%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E)
- [08 总结与展望](https://github.com/chirpyjay/BioRec-One/wiki/08-%E6%80%BB%E7%BB%93%E4%B8%8E%E5%B1%95%E6%9C%9B)

------

## License

MIT

## 免责声明

本项目仅供学习与教学演示，**不构成任何医疗建议，不得用于临床诊断或医疗目的**。

本设备未经医疗器械认证，作者不对因复刻、使用或改造本项目而造成的任何人身伤害、财产损失或其他安全事故承担责任。复刻前请确保您具备相应的电子和安全知识。
