# ESP32-C5 Factory Rack Positioning System

这个仓库现在已经从 ESP-IDF 的 `hello_world` 模板扩展为一个“工厂料架定位系统”示例工程，目标是：

- 终端基于 `ESP32-C5-WROOM-1`
- 周期扫描周边 5 GHz Wi-Fi AP
- 采集 `BSSID / RSSI / 信道`
- 连接回传网络并通过 HTTP 上传
- 由上位系统根据 AP 坐标完成定位

## 仓库结构

```text
.
├── docs
│   └── system_design_zh.md
├── hardware
│   ├── factory_locator_bom.csv
│   └── factory_locator_schematic.md
├── main
│   ├── CMakeLists.txt
│   └── factory_locator_main.c
├── server-java
│   ├── pom.xml
│   └── src
│       ├── main
│       └── test
└── server
    ├── app.py
    ├── positioning.py
    ├── mock_data
    │   ├── ap_registry.json
    │   └── scan_example.json
    └── tests
        └── test_positioning.py
```

## 重点文件

- 总体设计文档：
  [system_design_zh.md](/D:/coding/C/esp-32-c5/esp-rps/docs/system_design_zh.md)
- 固件主程序：
  [factory_locator_main.c](/D:/coding/C/esp-32-c5/esp-rps/main/factory_locator_main.c)
- 上位定位服务：
  [FactoryLocatorApplication.java](/D:/coding/C/esp-32-c5/esp-rps/server-java/src/main/java/com/factory/locator/FactoryLocatorApplication.java)
- Spring Boot 定位控制器：
  [LocationController.java](/D:/coding/C/esp-32-c5/esp-rps/server-java/src/main/java/com/factory/locator/controller/LocationController.java)
- 定位算法：
  [PositioningService.java](/D:/coding/C/esp-32-c5/esp-rps/server-java/src/main/java/com/factory/locator/service/PositioningService.java)
- 硬件 BOM：
  [factory_locator_bom.csv](/D:/coding/C/esp-32-c5/esp-rps/hardware/factory_locator_bom.csv)

## 快速开始

### 1. 运行 Spring Boot 上位系统

```powershell
cd server-java
mvn spring-boot:run
```

### 2. 运行定位算法测试

```powershell
cd server-java
mvn test
```

### 3. 编译固件

在已经配置好 `ESP-IDF` 环境后：

```powershell
idf.py set-target esp32c5
idf.py build
idf.py flash monitor
```

## 说明

- 当前硬件部分提供了可直接用于画板的连接关系、BOM 和 PCB 约束，但还不是 KiCad/Altium 原生工程文件。
- 当前 Java 上位系统已经是标准 Spring Boot Web 项目骨架，便于后续接入 MySQL、Redis、MQ 和权限系统。
- 原来的 `server` Python 目录保留为算法对照和快速原型，不影响你正式采用 Java。
