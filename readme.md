# 🕒 ESP32-S3 Smart Watch 智能手表项目

**中文 / English 双语说明**

本项目基于 **ESP-IDF v5.3.4** 开发，主控芯片为 **ESP32-S3**，配备 16 MB SPI Flash 与 8 MB PSRAM。  
智能手表集成了 **LVGL 图形界面、Wi-Fi 网络功能、摄像头拍照、SD 卡浏览与图像显示** 等功能，展示了 ESP32-S3 在 IoT 智能设备上的综合能力。

This project is developed based on **ESP-IDF v5.3.4**, using **ESP32-S3** as the main controller, with 16 MB SPI Flash and 8 MB PSRAM.  
The smart watch integrates **LVGL GUI, Wi-Fi connectivity, camera capture, SD card browsing, and image display**, demonstrating the comprehensive capabilities of ESP32-S3 in IoT devices.

---

## 🧩 项目特性 Features

### 📶 Wi-Fi 网络功能 / Wi-Fi Network

- 支持连接 Wi-Fi 热点 / Connect to Wi-Fi networks  
- 获取 **网络时间 (NTP)** 并显示 / Synchronize time with NTP and display in UI  
- 调用[心知天气] API，展示当前天气与温度 / Fetch weather data via API and show temperature & weather info  

### 🖼️ LVGL 图形界面 / LVGL GUI

- 基于 [LVGL](https://lvgl.io) 构建界面 / GUI built using LVGL library  
- **画布(Canvas)** 功能：触摸绘制实时显示 / Canvas feature: draw with touch in real time  
- 支持图标、文本、按钮、图片显示 / Support for icons, text, buttons, and image rendering  

### 💾 SD 卡浏览与图片显示 / SD Card Browser & Image Display

- 支持 FAT 文件系统挂载 / Mount FAT filesystem  
- 浏览 SD 卡目录结构 / Browse SD card file structure  
- 显示 **BMP 图片** / Display BMP images  
- 提供基础文件选择交互 UI / Basic file selection UI  

### 📸 摄像头功能 / Camera Feature

- 使用 **GC0308 摄像头模组** / GC0308 camera module  
- 拍照并保存至 SD 卡 BMP 文件 / Capture photo and save as BMP on SD card  
- 支持拍摄预览 / Preview support before saving  

---

## ⚙️ 硬件配置 Hardware

| 模块 Module | 型号 / Description |
|-------------|------------------|
| MCU | ESP32-S3 |
| Flash | 16 MB SPI Flash |
| PSRAM | 8 MB |
| 显示屏 st7789 LCD | SPI 接口 LCD 支持 LVGL / SPI LCD supporting LVGL |
| 触摸屏 ft6336 Touch | I²C 电容触摸屏 / I²C capacitive touch |
| 摄像头 GC0308 Camera | 
| 存储 Storage | MicroSD 卡 / MicroSD card |


## 🧠 软件架构 Software Architecture




## 🛠️ 开发环境 Development Environment

* **操作系统 / OS:** Ubuntu 20.04 LTS
* **ESP-IDF 版本 / ESP-IDF Version:** v5.3.4
* **工具链 / Toolchain:** xtensa-esp32s3-elf
* **构建系统 / Build System:** CMake + Ninja
* **开发工具 / IDE:** VSCode / CLion / Terminal

---

## 🚀 快速开始 Quick Start

1. **克隆仓库 / Clone repository**

   ```bash
   git clone https://github.com/survivorhao/test_repository.git


2. **设置 ESP-IDF 环境 / Set up ESP-IDF**

   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

3. **配置项目 / Configure project**

   ```bash
   idf.py menuconfig
   ```

4. **编译与烧录 / Build & Flash**

   ```bash
   idf.py build flash monitor
   ```

---

## 📷 效果展示 Preview

| 功能 / Feature         | 示例 / Demo                           |
| -------------------- | ----------------------------------- |
| 网络时间 / NTP           | 🕓 实时显示 / Realtime clock            |
| 天气信息 / Weather       | 🌤️ 天气 & 温度 / Weather & temperature |
| 画布绘图 / Canvas        | ✏️ 触摸即画 / Draw by touch             |
| SD卡图片浏览 / SD Browser | 🖼️ 显示 BMP / Display BMP images     |
| 拍照存储 / Camera        | 📸 BMP 保存 / Save photo as BMP       |



## 📚 后续计划 TODO

* [ ] 增加 BLE 通信模块 / Add BLE communication
* [ ] 增加 心率/计步传感器 / Heart rate & step sensor support
* [ ] 优化 LVGL UI 动画与页面切换 / Optimize LVGL animations
* [ ] 支持 JPG 图像格式 / Add JPG support

---

## 🧾 License

本项目遵循 [MIT License](LICENSE) / MIT License

---

## ✨ 作者 Author

**Survivorhao**
📧 [2737278737@qq.com](mailto:2737278737@qq.com)
🔗 GitHub: [https://github.com/survivorhao](https://github.com/survivorhao)




