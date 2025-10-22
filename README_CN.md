
[English Version](README.md)

# ESP32-S3 智能手表

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ESP-IDF 版本](https://img.shields.io/badge/ESP--IDF-release%2Fv5.4.2-green.svg)](https://github.com/espressif/esp-idf)

## 概述

本项目是一个基于 ESP32-S3 微控制器的开源智能手表开发项目。使用 ESP-IDF 框架（release/v5.4.2 版本），开发板为 LCSC-boards LCKFB-SZPI-ESP32-S3-VA（可从 [LCSC](https://www.lcsc.com/) 或 [SZLCSC](https://www.szlcsc.com/) 获取）。图形用户界面 (GUI) 采用 LVGL 实现平滑渲染和交互。

架构强调模块化和可扩展性：
- 使用 ESP event loop 实现组件解耦，采用发布者/订阅者模型。发布者无需关心订阅者，反之亦然。
- 使用单一 LVGL 任务确保 LVGL API 的线程安全，通过消息队列处理 UI 请求。

## 功能

- **WiFi 管理**：支持开启/关闭、扫描和连接网络。获取 IP 地址后，自动发送 SNTP 请求同步时间至本地 RTC。同时通过 HTTPS 获取天气数据（仅限中国地区）并更新 UI。
- **BLE HID 设备**：通过 BLE GATT 实现标准 HID 协议。在屏幕显示配对 PIN 码；加密连接后，实现音量加减、歌曲切换、播放/暂停。
- **SD 卡文件浏览器**：读取并使用 LVGL 部件列表显示 SD 卡内容。支持目录切换和 BMP 图片查看。
- **摄像头集成**：驱动 GC0308 摄像头，在 SPI LCD 上实时预览。支持拍照（按 BOOT0 键），保存为 BMP 格式至 SD 卡指定目录。
- **简易计算器**：使用双栈（运算符栈和操作数栈）处理基本算术 +, -, *, /,(,)，支持多级嵌套表达式如 "(1*((6+2)/4)-6)"。
- **设置界面**：配置系统选项，如天气 API 位置、屏幕亮度调节、内部 RAM 和 PSRAM 使用情况监控（若启用）、SD 卡信息（挂载状态和容量），自动睡眠时间配置，删除摄像头保存的图片。
- **日历和画布**：基本日历视图和简单绘图画布。
- **深度睡眠模式**：指定时间内无触摸事件，自动进入deep sleep mode，仅保留 RTC 电源域；RTC 时钟继续运行。唤醒后 UI 时间正常，无需重新 SNTP 同步。

## 注意事项

- **SD 卡浏览**：若目录下文件过多，将无法完整列出所有内容，这是由于 LVGL 部件的内存分配受限于内部 SRAM，可能引发程序崩溃。此外，SD 卡文件浏览器仅支持显示 BMP 格式图片（兼容 16/24/32 位颜色深度）。解码后的图片会临时存放在 PSRAM 中，并在显示后立即释放，因此无需担心 RAM 占用问题。
- **资源限制**：鉴于 ESP32-S3 的内部 RAM 容量有限，WiFi、BLE 和摄像头功能无法同时启用。代码中已内置强制限制机制，以避免潜在冲突。
- **LVGL 版本**：本项目采用 LVGL 8.3.11 版本。若需升级至 LVGL 9 或更高版本，请自行处理潜在的版本兼容性问题。
- **组件依赖**：在 `/main/idf_component.yml` 文件中已声明所有外部组件依赖。这些依赖将在执行 IDF 命令时自动从 [https://components.espressif.com](https://components.espressif.com) 下载。请勿擅自修改，以免引发兼容性问题。
- **其他注意**：（如需补充额外事项，可在此处添加。）

## 演示图片

![演示图片 1](asset/demo1.jpg)  
![演示图片 2](asset/demo2.jpg)  
![演示图片 3](asset/demo3.jpg)  
![演示图片 4](asset/demo4.jpg)

## 常见问题 (FAQ)

1. **可以使用其他 ESP-IDF 版本吗？**  
   不推荐。需手动解决版本依赖问题，且旧版本有 Bug。例如，IDF v5.2 存在 I2C 中断导致看门狗超时问题。版本切换参考官方文档。

2. **配置环境太麻烦了，我只想把程序烧录到我的 LCSC-boards LCKFB-SZPI-ESP32-S3-VA 开发板测试效果？**  
   可以！打开 `/bin/flash_project_args` 文本文件，其中定义的下载参数依次填写到官方 flash 下载工具即可。
   参考[这里的下载参数配置演示图](asset/flash_download.png)
   [https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32s3/production_stage/tools/flash_download_tool.html](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32s3/production_stage/tools/flash_download_tool.html)

## 相关资源

- **BLE HID 设备问题**：使用 NimBLE 栈时，初始化后反初始化 BLE 可能导致崩溃。参考：  
  [https://github.com/espressif/esp-idf/issues/17493#issue-3353465850](https://github.com/espressif/esp-idf/issues/17493#issue-3353465850)
- **BLE HID 参考资料**：  
  [HID Usage Tables](https://usb.org/document-library/hid-usage-tables-16)  
  [Bluetooth Core Specification 5.0](https://www.bluetooth.com/specifications/specs/core-specification-5-0/)  
  [USB HID Device Class Definition](https://www.usb.org/document-library/device-class-definition-hid-111)
- **ESP-IDF 官方文档**：  
  [ESP-IDF for ESP32-S3 (v5.4)](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/index.html)  
  [Espressif Components Registry](https://components.espressif.com/)
- **心知天气 API**：  
  [https://seniverse.yuque.com/hyper_data/api_v3](https://seniverse.yuque.com/hyper_data/api_v3)

## 安装

请先确保你已经安装好了 ESP-IDF 指定版本以及配套工具，如果没有请参考官方教程。

1. 克隆仓库：  
   ```
   git clone https://github.com/survivorhao/esp32s3watch.git
   cd esp32s3watch
   ```
2. 设置目标：
   ```
   idf.py set-target esp32s3
   ```

3. 配置：(构建系统会自动读取 /sdkconfig.defaults 文件)，你无需在配置任何项  
   ```
   idf.py menuconfig
   ```
4. 使用 本地项目路径/lv_conf.h 替换 本地项目路径/managed_components/lvgl/lv_conf_remplate.h 文件
   来配置lvgl相关设置。
   按照/asset/目录下的 errorX.png 和errorX_fix.png 修改指定文件。
   
5. 构建并烧录：
   ```
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor  # 替换为你的端口
   ```

## 贡献

欢迎贡献！Fork 仓库，创建分支，并提交拉取请求。
如果你对这个项目感兴趣或者遇到了什么问题可以添加QQ群聊1029609832或者直接发送邮件给2059931521@qq.com
## 许可证

MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。