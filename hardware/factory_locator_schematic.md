# ESP32-C5 工厂料架定位终端原理图说明

## 1. 电源

- `J1 USB-C VBUS` -> `F1 500mA PPTC` -> `D1 TVS` -> `U2 TLV75533` 输入
- `U2 3V3` -> `L1` -> `ESP32-C5-WROOM-1 3V3`
- `U2 IN/OUT` 两侧各放置 `10uF + 0.1uF`
- `ESP32-C5` 每个电源脚附近放置 `0.1uF`，距离小于 2 mm
- 电池版本增加 `J3 Li-ion` -> `U4 MCP73831` -> `SYS_3V7`

## 2. ESP32-C5-WROOM-1 核心连接

- `EN`：`10k` 上拉到 `3V3`，并联 `0.22uF` 到 `GND`，`SW1` 下拉复位
- `GPIO9/BOOT`：`10k` 上拉到 `3V3`，`SW2` 下拉到 `GND`
- `U0TXD/U0RXD`：连接 `CP2102N TXD/RXD`
- `GPIO18`：状态 LED
- `GPIO4`：预留 I2C SDA，可扩展 IMU/温湿度
- `GPIO5`：预留 I2C SCL
- `GPIO2`：电池电压 ADC 采样

## 3. USB-UART 下载

- `USB-C D+ / D-` -> `ESD` -> `499R` 串联 -> `CP2102N`
- `CP2102N TXD` -> `ESP32-C5 RXD0`
- `CP2102N RXD` -> `ESP32-C5 TXD0`
- `CP2102N DTR/RTS` 可选接自动下载电路

## 4. 关键设计建议

- 若使用模组自带 PCB 天线，天线正前方、上下层、边缘外延 15 mm 范围禁布铜与器件
- 模组放在 PCB 边缘，天线朝向厂房开放区，不要朝向金属料架
- 地平面完整，但天线 keep-out 区域必须完全净空
- USB 和射频区域分离，`CP2102N` 放在板另一侧

## 5. 建议板框

- 开发板尺寸：`70 mm x 45 mm`
- 4 层板：
  - L1: 信号 + 关键电源
  - L2: 整面 GND
  - L3: 3V3 / 辅助信号
  - L4: 信号

## 6. 典型接口定义

- `J2.1` 3V3
- `J2.2` GND
- `J2.3` TXD0
- `J2.4` RXD0
- `J2.5` EN
- `J2.6` GPIO9/BOOT
