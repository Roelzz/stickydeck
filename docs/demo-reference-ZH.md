# reTerminal Sticky Dashboard Demo

[English](README.md)

这是一个用于开发 reTerminal Sticky 板载硬件的 ESP-IDF Demo。它通过一个完整可运行的 Dashboard 应用，演示电子纸屏幕、实体按键、触摸屏、环境传感器、RTC、电池计量、MicroSD 卡、IMU、麦克风、蜂鸣器和深度睡眠。

你既可以直接烧录并体验完整 Demo，也可以参考其中的设备模块开发自己的 Sticky 应用。工程将硬件访问、应用状态和页面绘制分开，各个驱动模块可以独立复用，不需要复制整套 UI。

## 功能

Dashboard 包含六个页面：

| 页面 | 功能 |
| --- | --- |
| Home | 文字、线条、矩形和四阶灰度 |
| Sensor | SHT40 温度和湿度 |
| Battery | BQ27220 电池电量和外部电源状态 |
| Note | 读取 MicroSD 卡中的 `/TEST.TXT` |
| IMU | 由 LSM6DS3 加速度计驱动的重力方向箭头 |
| Microphone | 一秒 PDM 采样，以及 RMS、峰值和音量条 |

PCF8563 时间显示在 Sensor、Battery、Note、IMU 和 Microphone 页面的公共状态栏中，并且每分钟通过一次黑白局刷更新。Demo 还支持按键和触摸翻页、蜂鸣器反馈，以及恢复原页面状态的深度睡眠。

## 准备工作

- reTerminal Sticky
- USB-C 数据线
- ESP-IDF v5.4
- 用于 Note 页面的 FAT 格式 MicroSD 卡（可选）

工程配置：

- 目标芯片：ESP32-S3
- Flash：32 MB
- PSRAM：80 MHz Octal PSRAM
- 逻辑屏幕分辨率：800 x 480

如果尚未安装 ESP-IDF，请参考官方的
[ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4/esp32s3/get-started/)。

## 编译和运行

在工程目录中打开 ESP-IDF v5.4 终端，然后执行：

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为设备串口，例如 Windows 上的 `COM5`。按 `Ctrl+]` 退出串口监视器。

如果自动生成的构建文件出现不一致，只删除自动生成的 `build/` 目录，然后重新构建。

### 准备 MicroSD 卡

使用 FAT 文件系统格式化 MicroSD 卡，并在卡的根目录创建：

```text
TEST.TXT
```

示例内容：

```text
Hello Sticky! Happy coding!
```

Demo 最多读取 256 个字符。内置点阵字体面向可打印 ASCII 字符，因此建议使用 ASCII 文本。

## 操作方式

| 输入 | 功能 |
| --- | --- |
| `UP` 按键 | 上一页 |
| `DOWN` 按键 | 下一页 |
| 向右滑动 | 上一页 |
| 向左滑动 | 下一页 |
| 短按 `AI` | 刷新当前页面 |
| 长按 `AI` 约两秒 | 蜂鸣器提示后进入深度睡眠 |
| 睡眠时按下 `AI` | 唤醒并恢复睡眠前的页面 |

触摸只识别水平滑动。单击、短距离移动以及以垂直方向为主的手势会被忽略，以减少误触翻页。

## 工程结构

```text
reTerminal-Sticky-Dashboard-Demo/
|-- components/                 第三方组件和屏幕组件
|   |-- bq27220/
|   |-- button/
|   |-- gt911/
|   `-- seeed_epaper/
`-- main/
    |-- main.cpp                启动流程和主事件循环
    |-- pin_config.h            Sticky 引脚和 I2C 地址定义
    |-- board/                  共享电源、GPIO 和传感器 I2C 资源
    |-- devices/                硬件设备模块
    |-- app/                    状态、数据更新、事件和页面导航
    |-- pages/                  页面绘制函数
    `-- ui/                     Canvas、字体和公共状态栏
```

主要数据流：

```text
硬件 -> devices -> app_data -> AppState -> pages -> Canvas -> display
```

- `board/` 管理多个设备共用的资源。
- `devices/` 初始化硬件并返回读取结果。
- `app/app_data.cpp` 将设备读取结果写入 `AppState`。
- `pages/` 读取 `AppState` 并绘制页面，不直接访问硬件。
- `app/app.cpp` 处理事件、页面导航、绘制和屏幕刷新。

`main.cpp` 展示了正确的初始化顺序，是理解完整应用最合适的入口。

## 设备 API

每个板载外设都在 `main/devices/` 中提供了精简 API：

| 模块 | 主要 API | 用途 |
| --- | --- | --- |
| `sticky_display` | `init`、`canvas`、`refresh`、`refresh_partial`、`sleep` | 电子纸 Framebuffer 和刷新 |
| `sticky_buttons` | `init` | UP、DOWN 和 AI 按键事件 |
| `sticky_touch` | `init`、`stop` | GT911 滑动事件 |
| `sticky_buzzer` | `init`、`beep` | 简短操作提示音 |
| `sticky_sht40` | `init`、`read` | 温度和湿度 |
| `sticky_rtc` | `init`、`read` | 只读日期和时间 |
| `sticky_battery` | `init`、`read` | 电池剩余电量 |
| `sticky_charger` | `init`、`read` | 外部电源状态 |
| `sticky_sdcard` | `init`、`read_text` | 从 MicroSD 读取文本文件 |
| `sticky_imu` | `init`、`start_monitoring`、`get_state` | 稳定设备方向 |
| `sticky_microphone` | `init`、`capture` | 单次 PDM 音频测量 |
| `sticky_power` | `enter_deep_sleep` | 进入深度睡眠并通过 AI 按键唤醒 |

准确的数据结构、参数和返回值请查看对应的 `.h` 文件。运行时函数返回 `esp_err_t`，使用读取结果前应先检查返回值。

### 读取 I2C 传感器

电池计量、RTC、SHT40 和 IMU 使用 Board 层创建的共享传感器总线。初始化设备时需要传入现有的总线句柄：

```cpp
ESP_ERROR_CHECK(board_init());
ESP_ERROR_CHECK(sticky_sht40_init(board_sensor_i2c_bus()));

Sht40Reading reading = {};
if (sticky_sht40_read(reading) == ESP_OK) {
    printf("Temperature: %.1f C\n", reading.temperature_c);
    printf("Humidity: %.1f %%\n", reading.humidity_percent);
}
```

不要为单个传感器在 GPIO0/GPIO1 上重复创建 I2C 总线。

### 在电子纸屏幕上绘图

Display 模块持有 Framebuffer，并通过 `Canvas` 提供绘图接口：

```cpp
ESP_ERROR_CHECK(sticky_display_init());

Canvas *canvas = sticky_display_canvas();
canvas->clear(GrayLevel::White);
canvas->draw_text(60, 80, "Hello Sticky", 3, GrayLevel::Black);
canvas->draw_line(60, 130, 740, 130, GrayLevel::DarkGray);
canvas->draw_rect(60, 170, 260, 120, GrayLevel::Black);

ESP_ERROR_CHECK(sticky_display_refresh());
```

所有 UI 代码使用左上角为原点的 800 x 480 坐标系。物理屏幕所需的 180 度旋转已经由 `sticky_display` 处理。

Canvas 支持以下四种颜色：

```cpp
GrayLevel::Black
GrayLevel::DarkGray
GrayLevel::LightGray
GrayLevel::White
```

可以只更新 Canvas 中的一个逻辑区域，而不刷新整个屏幕：

```cpp
// 调用前只修改需要局刷的黑白 Canvas 像素。
ESP_ERROR_CHECK(sticky_display_refresh_partial());
```

局刷 API 会在内部完成物理屏幕的 180 度坐标转换。它使用电子纸的黑白局刷波形，因此发生变化的区域应只包含 `Black` 和 `White` 像素。Dashboard 只在黑白页面中使用该 API 更新状态栏的 RTC 时间。

### 显示应用数据

完整 Demo 不允许页面直接读取硬件。设备读取结果先写入 `AppState`，再由页面绘制：

```text
sticky_sht40_read()
-> update_environment(state)
-> state.environment
-> sensor_page_render(canvas, state)
-> sticky_display_refresh()
```

这样的分层可以让页面行为保持稳定，也方便在使用不同 UI 的应用中复用硬件模块。

如果需要增加页面，请添加新的 `PageId`，创建 `pages/*_page.cpp/.h` 绘制函数，在 `app/app.cpp` 中接入页面，并将源文件加入 `main/CMakeLists.txt`。页面需要硬件数据时，通过 `app_data` 更新状态。

## 重要硬件说明

### 共享 SPI 总线

电子纸屏幕和 MicroSD 卡共用 SPI2 信号线，但使用不同的片选引脚。屏幕首先初始化 SPI2；读取 SD 时，将卡临时挂载到已有总线，读取并关闭文件，再卸载文件系统，最后才刷新屏幕。

SD 模块不能释放 SPI2，因为屏幕仍然连接在同一总线上。修改 SPI 总线配置时，所有未使用的数据引脚必须保持为 `-1`；如果未使用引脚被零初始化，SPI 可能错误占用 GPIO0，导致传感器 I2C 时钟失效。

### I2C 总线

- GPIO0/GPIO1 上的 I2C1：BQ27220、PCF8563、SHT40 和 LSM6DS3。
- GPIO2/GPIO3 上的 I2C0：GT911 触摸控制器。

除非同时重新检查 GT911 的复位、地址选择和坐标转换，否则不要将 GT911 移到传感器总线上。

### 屏幕刷新

Home 页面使用四阶灰度全屏刷新：

```cpp
sticky_display_refresh();
```

Sensor、Battery、Note、IMU、Microphone 和深度睡眠页面使用黑白全屏刷新：

```cpp
sticky_display_refresh_monochrome();
```

黑白页面的 RTC 时间区域每分钟通过 `sticky_display_refresh_partial()` 更新一次。Home 页面不显示状态栏时间，其他页面内容不会在后台定时刷新。切换页面和短按 AI 刷新时，程序根据当前页面选择对应的全刷模式。

`app/app_clock.cpp` 将第一次定时触发对齐到 RTC 的下一分钟，并发送 `AppEvent::ClockMinuteTick`。定时器回调不会直接访问 RTC 或屏幕，读取时间和执行局刷仍由正常的 App 事件循环负责。

电子纸连续多次局刷后可能逐渐出现轻微残影，需要时可以短按 AI 进行一次普通全刷。电子纸全刷时出现短暂闪烁或旧画面属于正常现象。

### MicroSD 重新读取

每次进入 Note 页面或短按 AI 刷新时，程序都会重新读取 `TEST.TXT`。文件系统只在读取期间挂载，因此可以在两次读取之间拔出存储卡或修改文件。

### 深度睡眠

进入睡眠前，程序会将 `AppState` 保存到 RTC 内存、停止触摸轮询、绘制黑白睡眠页面、关闭屏幕电源，并将 GPIO4 配置为唤醒源。唤醒时 ESP32-S3 会重新启动，并恢复睡眠前显示的页面。

## 引脚定义

所有经过验证的引脚分配和 I2C 地址都定义在
[`main/pin_config.h`](main/pin_config.h) 中。开发应用时应以该文件为准，不要在业务代码中重复填写引脚号。

主要共享连接：

| 功能 | 引脚 |
| --- | --- |
| 传感器 I2C1 | SCL GPIO0，SDA GPIO1 |
| 触摸 I2C0 | SCL GPIO2，SDA GPIO3 |
| UP / DOWN / AI | GPIO5 / GPIO6 / GPIO4 |
| 电子纸 SPI | MOSI GPIO14，MISO GPIO12，CLK GPIO13，CS GPIO15 |
| MicroSD | CS GPIO8，检测 GPIO11，使能 GPIO10；与电子纸共用 SPI |
| PDM 麦克风 | CLK GPIO19，数据 GPIO20，使能 GPIO38 |

## 常见问题

- **传感器 I2C 事务失败：**确认 SPI 未使用引脚仍设置为 `-1`，并且没有在 GPIO0/GPIO1 上创建第二条总线。
- **触摸方向相反：**不要在页面代码中旋转坐标，保持 `sticky_touch` 内已经验证的转换。
- **MicroSD 文件修改后没有显示：**重新进入 Note 页面，或短按 AI 重新读取 `TEST.TXT`。
- **切换页面时屏幕闪烁：**这是当前电子纸全屏刷新的正常过程。
- **构建时提示缺少自动生成文件：**只删除 `build/`，然后重新执行 `idf.py build`。

## 扩展 Demo

增加硬件或 UI 功能时，请保持现有分层：

- 引脚定义放在 `pin_config.h`。
- 共享总线和板级电源时序放在 `board/`。
- 硬件访问放在 `devices/`。
- 在 `app_data` 中将读取结果转换为可显示状态。
- 页面只读取状态，并通过 `Canvas` 绘图。
- 输入和异步操作通过 `AppEvent` 传递。
- 屏幕和触摸方向转换保留在对应设备模块中。

遵守这些边界，就可以继续添加功能而不改变已有页面的行为。
