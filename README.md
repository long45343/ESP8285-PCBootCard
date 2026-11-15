# 简介

在ESP8285上开发的兼容巴法云和Home Assistant（HA）的开机卡固件

## 在线演示

不想看代码？这里有一个纯网页的演示版本，可以在浏览器里直接体验配网、登录和状态页面：

**[打开在线演示](https://long45343.github.io/ESP8285-PCBootCard/demo)**

演示是纯前端模拟，不连接真实设备。右下角面板可以手动触发开关机，观察状态和日志变化。

## 功能

- **远程开关机、重启**：远程控制Power_SW和Reset_SW实现远程开关机，电脑卡死也可以操作
- **状态回传**：电脑状态会回传到巴法云/HA
- **网页配网**：手机连上板子的热点，打开网页填 WiFi 密码就行，不用改代码
- **网页维护**：同一个局域网里打开板子网页，可以看运行日志、升级固件、切换 WiFi、改访问密码、切换巴法云 / Home Assistant 模式
- **状态灯**：配网慢闪、连网快闪、正常熄灭、出问题常亮

## 硬件

- 主控：ESP8285（1MB Flash）
- 插槽：PCIe x1
- 取电：PCIe 插槽的 3.3V 待机供电
- 外设：两路或四路光耦（分别接机箱电源键、重启键，四路光耦可以与主板开机键兼容）、一颗状态灯、一个板载按键

## 引脚

| 用途 | GPIO | 说明 |
|---|---|---|
| 电源键光耦 | 12 | 输出高电平约 500ms，模拟短按 |
| 重启键光耦 | 13 | 输出高电平约 500ms，模拟短按 |
| 状态灯 | 2 | 低电平点亮 |
| 开机状态检测 | 14 | 接 PCIe A11（PERST#），上拉输入 |
| 板载按键 | 4 | 长按 3 秒清空 WiFi 配置，重启进配网 |

## 编译

用 Arduino IDE 或者 arduino-cli，开发板选 **Generic ESP8285 Module**，参数如下：

```
Flash Size: 1MB (FS:64KB OTA:~470KB)
Flash Mode: DOUT
CPU Frequency: 80MHz
```

依赖的库：

- PubSubClient（MQTT）
- ArduinoJson（解析网页提交的数据和巴法云接口返回）

arduino-cli 命令行编译：

```bash
arduino-cli compile \
  --fqbn "esp8266:esp8266:esp8285:eesz=1M64" \
  --output-dir build \
  .
```

第一次必须用 USB 转串口线烧录。之后固件支持网页 OTA，可以直接在网页上传新的 `.bin` 升级，不用再拆机箱。

## 兼容其他芯片

代码本身没有绑定 ESP8285，换芯片的难度差别很大。

### ESP8266 系列：几乎不用改

ESP8285 本质上是“内置 Flash 的 ESP8266”，指令集、WiFi 协议栈、Arduino 库完全一样。

| 型号 | Flash | 说明 |
|---|---|---|
| ESP-12F / ESP-12S | 4MB | 最常用的模块，引脚比 8285 多 |
| NodeMCU | 4MB | 开发板形态，自带 USB 转串口 |
| Wemos D1 mini | 4MB | 同上，体积更小 |
| ESP-01 / ESP-01S | 1MB | 不推荐，只有 2 个可用引脚，接不了两路光耦 |

**代码一行都不用动，只改编译参数里的 Flash 大小。** 比如换成 ESP-12F：

```
Flash Size: 4MB (FS:1MB OTA:~1019KB)
Flash Mode: DOUT
CPU Frequency: 80MHz
```

对应的 arduino-cli 参数：

```bash
arduino-cli compile \
  --fqbn "esp8266:esp8266:generic:eesz=4M1M" \
  --output-dir build \
  .
```

OTA 空间从 470KB 变成约 1MB。注意 ESP8266 的 OTA 要把 Flash 分成两份固件，上限就是 1MB 左右，再大也上不去了。

### ESP32 系列：要改十几处

ESP32、ESP32-S2、ESP32-S3、ESP32-C3、ESP32-C6 都能跑，但底层库换了名字，需要改这些地方：

| 现在用的 | ESP32 上要换成 |
|---|---|
| `ESP8266WiFi.h` | `WiFi.h` |
| `ESP8266WebServer.h` | `WebServer.h` |
| `ESP8266mDNS.h` | `ESPmDNS.h` |
| `Updater.h` | `Update.h` |
| `EEPROM.h` | `Preferences.h` |
| `ESP.getChipId()` | `ESP.getEfuseMac()` |
| `WiFi.scanComplete()` | ESP32 没有这个函数，扫描要用事件回调 |

**换 ESP32 的好处**：Flash 至少 4MB，内存 520KB（现在只有 80KB），OTA 空间充足，还能加蜂鸣器、P80 自检这些功能。

**换 ESP32 的代价**：板子要重新画，引脚电压和封装都不一样，代码要花半天改。

### 不建议的芯片

- **其他 WiFi 芯片**（BK7231、RTL8710 等）：Arduino 支持不完善，很多库用不了，移植等于重写
- **没有 WiFi 的芯片**（Arduino UNO、STM32）：得外挂 ESP-01 当 WiFi 模块，两个芯片之间还要写串口通信协议，工作量翻好几倍，稳定性也差

## 配网

1. 第一次上电，或长按板载按键 3 秒，会开一个叫 `PCBoot-XXXX` 的热点
2. 连上这个热点，会自动弹出配置页面（没弹出就手动打开 `192.168.4.1`）
3. 选 WiFi、填密码，再选控制方式：
   - **巴法云**：填 32 位私钥，两个开关主题会自动创建，不用手动建。每次重新配网都要把私钥重填一遍
   - **Home Assistant**：填 MQTT 服务器地址和端口，三个实体会自动注册
4. 点保存，板子重启并连接

## 网页维护

板子和手机在同一个 WiFi 下时，浏览器打开 `http://pcboot.local` 或者板子的 IP 地址。

页面上能做这些事：

- 看设备状态（WiFi 信号、MQTT 连接、电脑开关机、剩余内存）
- 看运行日志（内存里存最近 3KB，正常运行时大约半小时）
- 上传 `.bin` 文件升级固件
- 扫描周围 WiFi 并切换
- 修改网页访问密码
- 切换巴法云 / Home Assistant 模式，不用为了改这个去拆机箱按按键


## 关于巴法云

- 私钥在[巴法云 MQTT 控制台](https://cloud.bemfa.com/mdevice.php)首页可以看到
- 两个主题名从板子的 MAC 地址后四位生成，比如 `pcpwswb2c3006`、`pcpwrstb2c3006`
- 主题名最后三位决定设备类型，`006` 是开关设备

## 关于 Home Assistant

板子会自动发 MQTT Discovery 消息，HA 里会出现三个实体：

- `button.电脑电源`：按一下开机或关机
- `button.电脑重启`：按一下重启
- `binary_sensor.电脑状态`：反映电脑真实开关机状态

电源做成按钮而不是开关，是因为物理上就是"按一下"，不是"保持某个状态"。电脑当前是开是关，看状态传感器就行。

## 注意

- 板子插在 PCIe 插槽上，取的是待机供电，所以电脑关机状态下板子也在工作
- 换 WiFi 时如果密码填错，板子会连不上，需要长按 3 秒重新配网
- 升级固件时不要断电，写坏了只能拆下来用 USB 线救
- 网页密码忘了，长按 3 秒进配网页面，把密码那一栏清空再保存。私钥那一栏要重新填一次

## 文件说明

| 文件 | 内容 |
|---|---|
| `LongBootCard.ino` | 主程序：配网、MQTT、按键控制、状态检测、网页服务 |
| `web_page.h` | 配网页面 |
| `status_page.h` | 运行状态页面（日志、OTA、WiFi 切换、改密码、切换连接模式） |
| `login_page.h` | 登录页面 |
