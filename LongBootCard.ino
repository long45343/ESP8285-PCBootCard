/*
 * ESP8285 电脑开机卡 v2
 *
 * 功能：
 *   - 现代化网页配网（WiFi + 巴法云私钥）
 *   - 自动创建巴法云主题（开关机 / 重启两个设备）
 *   - 巴法云 / Home Assistant 双模式
 *   - LED 状态指示
 *   - PCIe 开关机状态检测并上报
 *
 * 编译：
 *   开发板 Generic ESP8285 Module
 *   Flash Size 1MB (FS:64KB OTA:~470KB)
 *   Flash Mode DOUT
 *   CPU 80MHz
 *   库：PubSubClient、ArduinoJson
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "web_page.h"
#include "status_page.h"
#include "login_page.h"

// ============================================================
// 1. 引脚定义
// ============================================================
#define PIN_POWER    12    // 电源键光耦（物理引脚 10）
#define PIN_RESET    13    // 重启键光耦（物理引脚 12）
#define PIN_LED      2     // 指示灯
#define PIN_STATUS   14    // 开机状态检测，接 PCIe A11 (PERST#)
#define PIN_BUTTON   4     // 板载重置按键（长按 3 秒进配网）

#define LED_ACTIVE_LOW   1 // LED 低电平点亮（多数开发板如此）
#define STATUS_ACTIVE_HIGH 1 // PERST# 释放后为高电平，表示系统在运行

#define BUTTON_HOLD_MS   3000  // 长按多久进配网

// LED 状态机（放在文件开头，避免 Arduino 自动原型生成时找不到类型）
enum LedMode { LED_OFF, LED_SLOW, LED_FAST, LED_ON };

// ============================================================
// 1b. 日志：同时写串口和内存，网页上能看
// ============================================================
// 板子装进机箱以后就够不着 USB 口了，串口日志等于看不见。
// 这里开一块内存当"环形记录本"，最新的日志盖掉最旧的，
// 网页通过 /api/log 把它读走。
// 注意：这段必须放在所有枚举/结构体之后，否则 Arduino 自动生成的
// 函数原型会插到它前面，导致类型找不到。
#define LOG_BUF_SIZE 3072

char logBuf[LOG_BUF_SIZE];
int  logHead = 0;          // 下一个要写的位置
bool logWrapped = false;   // 是否已经写满一圈

void logAppendChar(char c) {
  logBuf[logHead] = c;
  logHead = (logHead + 1) % LOG_BUF_SIZE;
  if (logHead == 0) logWrapped = true;
}

// 继承 Print，这样 print/println/printf 全都能用，写一份同时去两个地方
class LogPrint : public Print {
public:
  size_t write(uint8_t c) override {
    Serial.write(c);
    logAppendChar((char)c);
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    Serial.write(buf, size);
    for (size_t i = 0; i < size; i++) logAppendChar((char)buf[i]);
    return size;
  }
};

LogPrint Log;

// ============================================================
// 2. 配置存储
// ============================================================
#define CONFIG_ADDR   0x200
#define CONFIG_MAGIC  0xA5C5

struct Config {
  uint16_t magic;
  char wifiSSID[33];
  char wifiPass[65];
  char bemfaUid[40];
  char topicPower[40];
  char topicReset[40];
  uint8_t mode;          // 0=巴法云 1=HA
  char mqttHost[40];
  uint16_t mqttPort;
  char webPass[33];      // 网页访问密码，留空表示不鉴权
};

Config cfg;

// ============================================================
// 3. HA 模式的 Topic
// ============================================================
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_NODE_ID          "pcboot"
#define HA_UNIQUE_ID        "pcboot_esp8285"

char haTopicPowerCmd[64];
char haTopicResetCmd[64];
char haTopicStatusState[64];
char haTopicAvailability[64];

void setupHaTopics() {
  snprintf(haTopicPowerCmd,    sizeof(haTopicPowerCmd),    "%s/power/set",  HA_NODE_ID);
  snprintf(haTopicResetCmd,    sizeof(haTopicResetCmd),    "%s/reset/set",  HA_NODE_ID);
  snprintf(haTopicStatusState, sizeof(haTopicStatusState), "%s/status/state", HA_NODE_ID);
  snprintf(haTopicAvailability,sizeof(haTopicAvailability),"%s/availability", HA_NODE_ID);
}

// ============================================================
// 3. 全局对象
// ============================================================
ESP8266WebServer server(80);
DNSServer dnsServer;
WiFiClient net;
PubSubClient mqtt(net);

bool apMode = false;
bool lastPowerState = false;
bool stateReported = false;          // 这次启动后有没有把状态发出去过
unsigned long lastStatusCheck = 0;
unsigned long lastMqttRetry = 0;
unsigned long mqttRetryGap = 5000;   // MQTT 重连间隔，连不上就翻倍，最长 60 秒

// 待回发的消息。
// MQTT 回调是在“正在收数据”的那一刻被调用的，这时候往同一条连接写数据，
// 相当于一边接电话一边对着话筒说话，容易把连接搞乱。
// 所以回调里只登记要发什么，回到主循环再真正发出去。
char pubTopic[64] = {0};
char pubPayload[16] = {0};
bool pubRetain = false;
bool pubPending = false;

// LED 状态变量（枚举定义在文件开头）
LedMode ledMode = LED_OFF;
unsigned long lastLedToggle = 0;
bool ledState = false;

// ============================================================
// 4. 配置读写
// ============================================================
void loadConfig() {
  EEPROM.begin(1024);
  EEPROM.get(CONFIG_ADDR, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    cfg.mode = 0;
    cfg.mqttPort = 1883;      // 端口给个标准默认值，地址留空由配网时填
    EEPROM.put(CONFIG_ADDR, cfg);
    EEPROM.commit();
    Log.println("[CFG] 首次启动，使用默认配置");
  }
  // 旧固件没有 webPass 这个字段，升级后这里读到的是 Flash 里的残留字节。
  // 只认可见 ASCII，碰到乱码就清空，免得莫名其妙被锁在门外。
  bool badPass = false;
  bool terminated = false;
  for (unsigned int i = 0; i < sizeof(cfg.webPass); i++) {
    char c = cfg.webPass[i];
    if (c == 0) { terminated = true; break; }
    if (c < 0x20 || c > 0x7E) { badPass = true; break; }
  }
  // 33 个字节全被占满、结尾没有 0，后面当字符串用就会读到数组外面去
  if (!terminated) badPass = true;
  if (badPass) {
    cfg.webPass[0] = 0;
    EEPROM.put(CONFIG_ADDR, cfg);
    EEPROM.commit();
    Log.println("[CFG] 网页密码字段异常，已清空");
  }

  // 私钥只打印前 4 位，避免日志泄露完整密钥
  if (cfg.bemfaUid[0]) {
    Log.printf("[CFG] SSID=%s UID=%.4s**** mode=%d\n",
                  cfg.wifiSSID[0] ? cfg.wifiSSID : "(空)",
                  cfg.bemfaUid, cfg.mode);
  } else {
    Log.printf("[CFG] SSID=%s UID=(空) mode=%d\n",
                  cfg.wifiSSID[0] ? cfg.wifiSSID : "(空)", cfg.mode);
  }
}

void saveConfig() {
  cfg.magic = CONFIG_MAGIC;
  EEPROM.put(CONFIG_ADDR, cfg);
  EEPROM.commit();
  Log.println("[CFG] 已保存");
}

// ============================================================
// 5. LED 控制
// ============================================================
void setLed(bool on) {
  ledState = on;
#if LED_ACTIVE_LOW
  digitalWrite(PIN_LED, on ? LOW : HIGH);
#else
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#endif
}

void setLedMode(LedMode m) {
  ledMode = m;
  if (m == LED_OFF) setLed(false);
  if (m == LED_ON) setLed(true);
}

void updateLed() {
  unsigned long now = millis();
  unsigned long interval = 0;
  if (ledMode == LED_SLOW) interval = 500;
  else if (ledMode == LED_FAST) interval = 125;
  else return;
  if (now - lastLedToggle >= interval) {
    lastLedToggle = now;
    setLed(!ledState);
  }
}

// ============================================================
// 6. 按键控制（非阻塞）
// ============================================================
// 原来用 delay(ms) 会把主循环卡住，期间状态检测、MQTT、按键检测全部停摆。
// 现在改成：按下后记下该松开的时刻，由 loop 里的 updatePress() 收尾。
int pressPin = -1;
unsigned long pressReleaseAt = 0;

bool pressBusy() {
  // 兜底：正常情况下 updatePress() 到点就会松手。
  // 万一主循环被别的事情卡住过，这里再补一道保险——
  // 超过 5 秒一律当作已经松手，免得后续指令被永久丢弃。
  // 注意必须转成有符号再比：pressReleaseAt 在未来，无符号相减会回绕成大正数，
  // 那样一按下就判定"超时"，按键会被立刻掐断。
  if (pressPin >= 0 && (long)(millis() - pressReleaseAt) > 5000) {
    digitalWrite(pressPin, LOW);
    pressPin = -1;
    Log.println("[KEY] 超时强制松手");
  }
  return pressPin >= 0;
}

void pressStart(int pin, int ms) {
  if (pressPin >= 0) digitalWrite(pressPin, LOW);   // 上一次还没收尾，先放掉
  digitalWrite(pin, HIGH);
  pressPin = pin;
  pressReleaseAt = millis() + ms;
}

void updatePress() {
  if (pressPin >= 0 && (long)(millis() - pressReleaseAt) >= 0) {
    digitalWrite(pressPin, LOW);
    pressPin = -1;
  }
}

// 回调里只登记，不真发
void queuePublish(const char* topic, const char* payload, bool retain) {
  strncpy(pubTopic, topic, sizeof(pubTopic) - 1);
  pubTopic[sizeof(pubTopic) - 1] = 0;
  strncpy(pubPayload, payload, sizeof(pubPayload) - 1);
  pubPayload[sizeof(pubPayload) - 1] = 0;
  pubRetain = retain;
  pubPending = true;
}

// 回到主循环再真发
void flushPublish() {
  if (!pubPending) return;
  pubPending = false;
  if (!mqtt.connected()) {
    Log.printf("[MQTT] 回发跳过（未连接）%s=%s\n", pubTopic, pubPayload);
    return;
  }
  bool ok = mqtt.publish(pubTopic, pubPayload, pubRetain);
  Log.printf("[MQTT] 回发 %s=%s %s\n", pubTopic, pubPayload, ok ? "成功" : "失败");
}

// ============================================================
// 7. 状态检测
// ============================================================
bool readPowerState() {
#if STATUS_ACTIVE_HIGH
  return digitalRead(PIN_STATUS) == HIGH;
#else
  return digitalRead(PIN_STATUS) == LOW;
#endif
}

// ============================================================
// 7b. 板载物理按键：长按 3 秒清空 WiFi 配置，重启进配网
// ============================================================
void checkButton() {
  static bool pressed = false;
  static unsigned long btnDownAt = 0;   // 别叫 pressStart，会遮蔽同名全局函数

  bool now = (digitalRead(PIN_BUTTON) == LOW);   // 按键按下拉低

  if (now) {
    if (!pressed) {
      pressed = true;
      btnDownAt = millis();
      Log.println("[BTN] 按下，按住 3 秒进配网");
    } else if (millis() - btnDownAt >= BUTTON_HOLD_MS) {
      Log.println("[BTN] 长按满 3 秒，清空 WiFi 配置并重启");
      setLedMode(LED_ON);
      // 只清 WiFi。私钥和模式保留，但配网页面要求重新填一次私钥，
      // 这样保存时不会因为留空把原来的私钥悄悄清掉。
      cfg.wifiSSID[0] = 0;
      cfg.wifiPass[0] = 0;
      saveConfig();
      delay(300);
      ESP.restart();
    }
  } else if (pressed) {
    Log.println("[BTN] 松开，未到 3 秒，取消");
    pressed = false;
  }
}

// ============================================================
// 8. 巴法云建主题
// ============================================================
int bemfaCreateTopic(const char* topic, int type, const char* name) {
  if (!net.connect("pro.bemfa.com", 80)) {
    Log.println("[BEMFA] 连接失败");
    return -1;
  }

  StaticJsonDocument<256> doc;
  doc["uid"] = cfg.bemfaUid;
  doc["topic"] = topic;
  doc["type"] = type;
  doc["name"] = name;

  String body;
  serializeJson(doc, body);

  net.print("POST /v1/createTopic HTTP/1.1\r\n");
  net.print("Host: pro.bemfa.com\r\n");
  net.print("Content-Type: application/json\r\n");
  net.print("Content-Length: ");
  net.print(body.length());
  net.print("\r\n\r\n");
  net.print(body);

  unsigned long t = millis();
  while (!net.available() && millis() - t < 5000) delay(10);
  if (!net.available()) {
    net.stop();
    Log.println("[BEMFA] 无响应");
    return -1;
  }

  // 网络数据是一段一段到的，不能一看到 available() 为 0 就以为读完了。
  // 改成连续 500ms 没有新数据才算结束。
  String resp;
  resp.reserve(512);
  t = millis();
  unsigned long lastData = millis();
  while (millis() - t < 5000) {
    if (net.available()) {
      resp += (char)net.read();
      lastData = millis();
    } else if (millis() - lastData > 500) {
      break;
    }
    delay(1);
  }
  net.stop();

  // \r\n\r\n 占 4 个字节，正文从 idx+4 开始
  int idx = resp.indexOf("\r\n\r\n");
  int skip = 4;
  if (idx < 0) {
    idx = resp.indexOf("\n\n");
    skip = 2;
  }
  String json = idx >= 0 ? resp.substring(idx + skip) : resp;
  json.trim();

  StaticJsonDocument<256> out;
  DeserializationError err = deserializeJson(out, json);
  if (err) {
    Log.printf("[BEMFA] 解析失败: %s\n", json.c_str());
    return -1;
  }

  int code = out["code"] | -1;
  Log.printf("[BEMFA] 建主题 %s -> code=%d\n", topic, code);
  return code;
}

// 从 MAC 派生两个主题名，多块板子不会撞车。
// 巴法云按主题名后三位识别设备类型：006=开关设备。
// 主题名只能用字母和数字，所以后缀取 MAC 后两字节的十六进制，共 4 位。
void setupBemfaTopics() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char id[5];
  bool allZero = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) { allZero = false; break; }
  }
  if (allZero) {
    // eFuse 异常时 MAC 会读成全 0，退回用芯片 ID，避免所有板子撞同一个主题名
    snprintf(id, sizeof(id), "%04x", ESP.getChipId() & 0xFFFF);
    Log.println("[BEMFA] MAC 读取异常，改用芯片 ID");
  } else {
    snprintf(id, sizeof(id), "%02x%02x", mac[4], mac[5]);
  }

  snprintf(cfg.topicPower, sizeof(cfg.topicPower), "pcpwsw%s006",  id);
  snprintf(cfg.topicReset, sizeof(cfg.topicReset), "pcpwrst%s006", id);

  Log.printf("[BEMFA] 唯一后缀 %s\n", id);
  Log.printf("[BEMFA] 电源=%s 重启=%s\n", cfg.topicPower, cfg.topicReset);
}

void ensureTopics() {
  if (cfg.mode != 0) return;          // HA 模式不需要巴法云主题
  if (cfg.bemfaUid[0] == 0) return;

  // 巴法云连不上时别死等。这里先设好超时，最坏也就卡几秒；
  // 不设的话用的是默认值，可能十几秒，那段时间网页打不开。
  net.setTimeout(2000);

  // 两个主题：开关机开关、重启开关。开关机那个兼做状态显示。
  bemfaCreateTopic(cfg.topicPower, 1, "开/关机");
  delay(500);
  bemfaCreateTopic(cfg.topicReset, 1, "重启电脑");
}

// ============================================================
// 8b. HA MQTT Discovery
// ============================================================
void publishHaDiscovery() {
  // ---- 电源（button）----
  // 做成按钮而不是开关：物理上就是短按一下电源键，
  // 电脑当前是开是关由下面的状态传感器反映，不需要开关来保持状态。
  {
    String topic = String(HA_DISCOVERY_PREFIX) + "/button/" HA_NODE_ID "/power/config";
    StaticJsonDocument<512> doc;
    doc["name"] = "电脑电源";
    doc["unique_id"] = HA_UNIQUE_ID "_power";
    doc["command_topic"] = haTopicPowerCmd;
    doc["payload_press"] = "PRESS";
    doc["availability_topic"] = haTopicAvailability;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    doc["icon"] = "mdi:power";
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = HA_UNIQUE_ID;
    dev["name"] = "电脑开机卡";
    dev["model"] = "ESP8285 PCIe";
    dev["manufacturer"] = "DIY";
    String payload;
    serializeJson(doc, payload);
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
    Log.printf("[HA] button(电源) 配置 %s (%d 字节)\n", ok ? "成功" : "失败", payload.length());
  }

  // ---- 重启（button）----
  {
    String topic = String(HA_DISCOVERY_PREFIX) + "/button/" HA_NODE_ID "/reset/config";
    StaticJsonDocument<512> doc;
    doc["name"] = "电脑重启";
    doc["unique_id"] = HA_UNIQUE_ID "_reset";
    doc["command_topic"] = haTopicResetCmd;
    doc["payload_press"] = "PRESS";
    doc["availability_topic"] = haTopicAvailability;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    doc["icon"] = "mdi:restart";
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = HA_UNIQUE_ID;
    String payload;
    serializeJson(doc, payload);
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
    Log.printf("[HA] button(重启) 配置 %s (%d 字节)\n", ok ? "成功" : "失败", payload.length());
  }

  // ---- 开关机状态（binary_sensor）----
  {
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" HA_NODE_ID "/status/config";
    StaticJsonDocument<512> doc;
    doc["name"] = "电脑状态";
    doc["unique_id"] = HA_UNIQUE_ID "_status";
    doc["state_topic"] = haTopicStatusState;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "power";
    doc["availability_topic"] = haTopicAvailability;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = HA_UNIQUE_ID;
    String payload;
    serializeJson(doc, payload);
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
    Log.printf("[HA] binary_sensor 配置 %s (%d 字节)\n", ok ? "成功" : "失败", payload.length());
  }
}

// ============================================================
// 9. MQTT 回调
// ============================================================
// 原地处理指令：转小写、去首尾空白、剥掉 /set 段和 ## 前缀。
// 直接改传进来的缓冲区，不新建 String，避免每条消息都动堆。
void normalizeCmd(char* msg) {
  for (char* p = msg; *p; p++) *p = (char)tolower((unsigned char)*p);

  char* s = msg;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;

  char* e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
  *e = 0;

  // 找最后一个 /set，跟原来的 lastIndexOf 行为保持一致
  char* lastSet = NULL;
  for (char* p = s; (p = strstr(p, "/set")) != NULL; p += 4) lastSet = p;
  if (lastSet) {
    s = lastSet + 4;
    while (*s == ' ') s++;
  }

  if (s[0] == '#' && s[1] == '#') {
    s += 2;
    while (*s == ' ') s++;
  }

  if (s != msg) memmove(msg, s, strlen(s) + 1);
}

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  // 指令都很短，64 字节足够；超长的直接截断，不往堆上分配
  if (len > 64) len = 64;
  char raw[65];
  memcpy(raw, payload, len);
  raw[len] = 0;
  normalizeCmd(raw);
  const char* cmd = raw;
  Log.printf("[MQTT] %s -> %s\n", topic, cmd);

  // 上一次按键还没松手，忽略新指令，避免叠加
  if (pressBusy()) {
    Log.println("[MQTT] 按键进行中，忽略本次指令");
    return;
  }

  char upTopic[48];

  if (cfg.mode == 0) {
    // ---------- 巴法云模式 ----------
    if (strcmp(topic, cfg.topicPower) == 0) {
      if (strcmp(cmd, "on") == 0 || strcmp(cmd, "1") == 0 || strcmp(cmd, "open") == 0) {
        Log.println(">> 开机");
        pressStart(PIN_POWER, 500);
        snprintf(upTopic, sizeof(upTopic), "%s/up", cfg.topicPower);
        queuePublish(upTopic, "on", true);
      } else if (strcmp(cmd, "off") == 0 || strcmp(cmd, "0") == 0 || strcmp(cmd, "close") == 0) {
        Log.println(">> 关机");
        pressStart(PIN_POWER, 1000);
        snprintf(upTopic, sizeof(upTopic), "%s/up", cfg.topicPower);
        queuePublish(upTopic, "off", true);
      }
    } else if (strcmp(topic, cfg.topicReset) == 0) {
      // 重启开关：点一下触发一次，然后立刻把云端状态复位成 off。
      // 不复位的话 App 上的开关会停在"开"，下次再点同一个位置不会有反应。
      if (strcmp(cmd, "on") == 0 || strcmp(cmd, "1") == 0 || strcmp(cmd, "reset") == 0) {
        Log.println(">> 重启");
        pressStart(PIN_RESET, 500);
      }
      snprintf(upTopic, sizeof(upTopic), "%s/up", cfg.topicReset);
      queuePublish(upTopic, "off", true);
    }
  } else {
    // ---------- HA 模式 ----------
    // 电源和重启都是按钮，收到就短按一次，不做状态判断
    if (strcmp(topic, haTopicPowerCmd) == 0) {
      Log.println(">> 按电源键");
      pressStart(PIN_POWER, 500);
    } else if (strcmp(topic, haTopicResetCmd) == 0) {
      Log.println(">> 按重启键");
      pressStart(PIN_RESET, 500);
    }
  }
}

// ============================================================
// 10. MQTT 连接
// ============================================================
// 把 PubSubClient 的错误码翻译成人话，排查时不用再查表
const char* mqttStateName(int s) {
  switch (s) {
    case -4: return "连接超时";
    case -3: return "连接丢失";
    case -2: return "TCP 连不上";
    case -1: return "未连接";
    case  0: return "已连接";
    case  1: return "协议版本不支持";
    case  2: return "客户端 ID 被拒";
    case  3: return "服务器不可用";
    case  4: return "用户名或密码错";
    case  5: return "未授权";
    default: return "未知";
  }
}

void connectMqtt() {
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(60);
  // 连不上时别死等。默认超时太长了，服务器不可达时每次重连会卡住
  // 主循环好几秒，网页请求全被堵住，表现就是"网页打不开"。
  net.setTimeout(2000);

  // 缓冲区只需要分配一次。每次重连都调 setBufferSize 会触发 realloc，
  // 堆紧张的时候还可能挪动内存，完全没必要。
  static bool bufferReady = false;
  if (!bufferReady) {
    if (!mqtt.setBufferSize(768)) {
      Log.println("[MQTT] 缓冲区分配失败");
    }
    bufferReady = true;
  }

  if (cfg.mode == 0) {
    // ---------- 巴法云 ----------
    if (cfg.bemfaUid[0] == 0) return;
    mqtt.setServer("bemfa.com", 9501);
    Log.print("[MQTT] 连接巴法云...");
    if (mqtt.connect(cfg.bemfaUid)) {
      Log.println("成功");
      // 只订阅两个开关主题；状态传感器是只读的，设备只往它发数据，不订阅
      bool s1 = mqtt.subscribe(cfg.topicPower);
      bool s2 = mqtt.subscribe(cfg.topicReset);
      Log.printf("[MQTT] 订阅 %s / %s %s\n", cfg.topicPower, cfg.topicReset,
                 (s1 && s2) ? "成功" : "失败");
      setLedMode(LED_OFF);

      // 连上后立即同步一次当前状态，避免云端显示的状态和实际不一致
      bool cur = readPowerState();
      lastPowerState = cur;
      stateReported = true;
      reportState(cur);
    } else {
      Log.printf("失败 rc=%d (%s)\n", mqtt.state(), mqttStateName(mqtt.state()));
      setLedMode(LED_ON);
    }
  } else {
    // ---------- Home Assistant ----------
    mqtt.setServer(cfg.mqttHost, cfg.mqttPort);

    Log.printf("[MQTT] 连接 HA %s:%d...", cfg.mqttHost, cfg.mqttPort);
    if (mqtt.connect(HA_UNIQUE_ID, nullptr, nullptr,
                     haTopicAvailability, 0, true, "offline")) {
      Log.println("成功");
      mqtt.publish(haTopicAvailability, "online", true);
      publishHaDiscovery();
      bool s1 = mqtt.subscribe(haTopicPowerCmd);
      bool s2 = mqtt.subscribe(haTopicResetCmd);
      Log.printf("[MQTT] 订阅 %s / %s %s\n", haTopicPowerCmd, haTopicResetCmd,
                 (s1 && s2) ? "成功" : "失败");
      setLedMode(LED_OFF);

      // 连上后立即同步一次当前状态。
      // 首次配对时 HA 刚创建实体，需要一条状态消息把初始值填上。
      // 如果不主动发，要等用户开关机一次才会更新。
      bool cur = readPowerState();
      lastPowerState = cur;
      stateReported = true;
      reportState(cur);
    } else {
      Log.printf("失败 rc=%d (%s)\n", mqtt.state(), mqttStateName(mqtt.state()));
      setLedMode(LED_ON);
    }
  }
}

// ============================================================
// 11. 状态上报
// ============================================================
void reportState(bool on) {
  if (!mqtt.connected()) return;
  if (cfg.mode == 0) {
    // 电源主题的状态就是电脑真实开关机状态，App 上的开关跟着它走
    char upPower[48];
    snprintf(upPower, sizeof(upPower), "%s/up", cfg.topicPower);
    bool ok = mqtt.publish(upPower, on ? "on" : "off", true);
    Log.printf("[STATE] 巴法云上报 %s%s\n", on ? "on" : "off", ok ? "" : " (失败)");
  } else {
    // HA 只有一个状态传感器，不再往 power/state 发（电源已经改成按钮了）
    const char* val = on ? "ON" : "OFF";
    bool ok = mqtt.publish(haTopicStatusState, val, true);
    Log.printf("[STATE] HA 上报 %s%s\n", val, ok ? "" : " (失败)");
  }
}

// ============================================================
// 12. Web 服务器
// ============================================================
void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  // 20 个网络用 StaticJsonDocument<1024> 装不下（每个约 85 字节），改到堆上分配
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  String out;
  serializeJson(arr, out);
  server.send(200, "application/json", out);
}

void handleSave() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"无数据\"}");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"格式错误\"}");
    return;
  }

  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  const char* uid = doc["uid"] | "";
  int mode = doc["mode"] | 0;
  const char* mqttHost = doc["mqttHost"] | "";
  int mqttPort = doc["mqttPort"] | 1883;
  const char* webPass = doc["webPass"] | "";

  if (strlen(ssid) == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"请选择 WiFi\"}");
    return;
  }
  if (mode != 0 && mode != 1) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"模式无效\"}");
    return;
  }
  if (mqttPort < 1 || mqttPort > 65535) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"端口应在 1-65535\"}");
    return;
  }
  // 私钥和 MQTT 地址都要求重新填一次，不接受留空。
  // 留空沿用旧值看着省事，实际用起来反而分不清到底用的是旧的还是新填的，
  // 所以宁可多填一次，也要让用户明确知道自己填了什么。
  if (mode == 0 && strlen(uid) != 32) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"私钥应为 32 位\"}");
    return;
  }
  if (mode == 1 && strlen(mqttHost) == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"请填写 MQTT 服务器地址\"}");
    return;
  }

  // strncpy 在源字符串正好占满时不会补结尾，这里显式补上
  strncpy(cfg.wifiSSID, ssid, sizeof(cfg.wifiSSID) - 1);
  cfg.wifiSSID[sizeof(cfg.wifiSSID) - 1] = 0;
  strncpy(cfg.wifiPass, pass, sizeof(cfg.wifiPass) - 1);
  cfg.wifiPass[sizeof(cfg.wifiPass) - 1] = 0;
  strncpy(cfg.bemfaUid, uid, sizeof(cfg.bemfaUid) - 1);
  cfg.bemfaUid[sizeof(cfg.bemfaUid) - 1] = 0;
  cfg.mode = mode;
  strncpy(cfg.mqttHost, mqttHost, sizeof(cfg.mqttHost) - 1);
  cfg.mqttHost[sizeof(cfg.mqttHost) - 1] = 0;
  cfg.mqttPort = mqttPort;
  // 密码前后容易带空格（手机输入法、复制粘贴都会），统一去掉再存
  String wp = String(webPass);
  wp.trim();
  strncpy(cfg.webPass, wp.c_str(), sizeof(cfg.webPass) - 1);
  cfg.webPass[sizeof(cfg.webPass) - 1] = 0;
  Log.printf("[CFG] 网页密码长度=%d\n", (int)strlen(cfg.webPass));
  saveConfig();

  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

void startConfigPortal() {
  apMode = true;
  setLedMode(LED_SLOW);

  String apName = "PCBoot-" + String(ESP.getChipId() & 0xFFFF, HEX);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());
  Log.printf("[AP] 热点: %s\n", apName.c_str());
  Log.printf("[AP] 地址: %s\n", WiFi.softAPIP().toString().c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  server.begin();
}

// ============================================================
// 12b. 正常运行时的小网页：看状态、看日志、OTA 升级
// ============================================================
// 鉴权用 Cookie。密码在配网页面设置，存在 EEPROM 里。
// 留空表示不设密码，谁都能进（家里局域网够用）。
// 算登录令牌。用固定缓冲区，不走 String，避免每个请求都动堆。
void makeToken(char* out, size_t outSize) {
  char s[80];
  snprintf(s, sizeof(s), "%s|%x", cfg.webPass, (unsigned)ESP.getChipId());
  uint32_t h = 2166136261u;
  for (char* p = s; *p; p++) {
    h ^= (uint8_t)*p;
    h *= 16777619u;
  }
  snprintf(out, outSize, "%x", (unsigned)h);
}

bool checkAuth() {
  if (cfg.webPass[0] == 0) return true;      // 没设密码，直接放行
  char token[16];
  makeToken(token, sizeof(token));
  char want[24];
  snprintf(want, sizeof(want), "auth=%s", token);
  String cookie = server.header("Cookie");
  return cookie.indexOf(want) >= 0;
}

void handleLogin() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"无数据\"}");
    return;
  }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"格式错误\"}");
    return;
  }
  const char* pass = doc["pass"] | "";
  String p = String(pass);
  p.trim();
  Log.printf("[WEB] 登录尝试：输入长度=%d 存储长度=%d\n",
             (int)p.length(), (int)strlen(cfg.webPass));
  if (strcmp(p.c_str(), cfg.webPass) != 0) {
    Log.println("[WEB] 登录失败：密码错误");
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"密码错误\"}");
    return;
  }
  char token[16];
  makeToken(token, sizeof(token));
  char cookie[64];
  snprintf(cookie, sizeof(cookie), "auth=%s; Path=/; Max-Age=86400; HttpOnly", token);
  server.sendHeader("Set-Cookie", cookie);
  server.send(200, "application/json", "{\"ok\":true}");
  Log.println("[WEB] 登录成功");
}

void handleLogout() {
  server.sendHeader("Set-Cookie", "auth=; Path=/; Max-Age=0");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatusPage() {
  if (!checkAuth()) {
    server.send_P(200, "text/html; charset=utf-8", LOGIN_HTML);
    return;
  }
  server.send_P(200, "text/html; charset=utf-8", STATUS_HTML);
}

void handleApiStatus() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  StaticJsonDocument<384> doc;
  doc["uptime"] = millis() / 1000;
  doc["wifi"]   = (WiFi.status() == WL_CONNECTED);
  doc["rssi"]   = WiFi.RSSI();
  doc["ip"]     = WiFi.localIP().toString();
  doc["mqtt"]   = mqtt.connected();
  doc["mode"]   = cfg.mode;
  doc["power"]  = lastPowerState;
  doc["heap"]   = ESP.getFreeHeap();
  doc["ssid"]   = WiFi.SSID();
  // WiFi 名字可能是中文。序列化成 JSON 时每个汉字会变成 \uXXXX 六个字节，
  // 32 个汉字就是 192 字节，再加上其他字段，320 不够用，给到 512。
  char out[512];
  serializeJson(doc, out, sizeof(out));
  server.send(200, "application/json", out);
}

// 日志分两段直接发出去，不拼成一个大 String。
// 之前是整块复制 3KB 到堆上，网页每 2 秒来一次，跑久了堆会碎。
void sendLogChunked() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain; charset=utf-8", "");
  if (!logWrapped) {
    if (logHead > 0) server.sendContent((const char*)logBuf, logHead);
  } else {
    server.sendContent((const char*)logBuf + logHead, LOG_BUF_SIZE - logHead);
    server.sendContent((const char*)logBuf, logHead);
  }
  server.sendContent("");   // 结束 chunked
}

void handleApiLog() {
  if (!checkAuth()) {
    server.send(401, "text/plain; charset=utf-8", "未登录");
    return;
  }
  sendLogChunked();
}

void handleApiRestart() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

// ---------- 联网状态下扫描并切换 WiFi ----------
// 扫描要占用射频，期间当前连接会短暂中断，扫完自动恢复。
// 所以用异步扫描：这里只启动，前端隔一秒来问一次结果，不卡主循环。
void handleApiScan() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  // 已经在扫了就不要再启动一次，否则会把上一轮的进度冲掉
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"ok\":true,\"running\":true}");
    return;
  }
  WiFi.scanDelete();
  int r = WiFi.scanNetworks(true);        // true = 后台扫描
  if (r == WIFI_SCAN_FAILED) {
    Log.println("[WIFI] 扫描启动失败");
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"扫描启动失败\"}");
    return;
  }
  Log.println("[WIFI] 开始扫描");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiScanResult() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"running\":true}");
    return;
  }
  if (n == WIFI_SCAN_FAILED) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"扫描失败\"}");
    return;
  }
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"]  = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  String out;
  serializeJson(arr, out);
  server.send(200, "application/json", out);
  WiFi.scanDelete();
  Log.printf("[WIFI] 扫描完成，%d 个网络\n", n);
}

void handleApiWifi() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"无数据\"}");
    return;
  }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"格式错误\"}");
    return;
  }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (strlen(ssid) == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"请选择网络\"}");
    return;
  }
  strncpy(cfg.wifiSSID, ssid, sizeof(cfg.wifiSSID) - 1);
  cfg.wifiSSID[sizeof(cfg.wifiSSID) - 1] = 0;
  strncpy(cfg.wifiPass, pass, sizeof(cfg.wifiPass) - 1);
  cfg.wifiPass[sizeof(cfg.wifiPass) - 1] = 0;
  saveConfig();
  Log.printf("[WIFI] 切换到 %s，准备重启\n", cfg.wifiSSID);
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

void handleApiPass() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"无数据\"}");
    return;
  }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"格式错误\"}");
    return;
  }
  const char* oldPass = doc["oldPass"] | "";
  const char* newPass = doc["newPass"] | "";

  // 已经设了密码的话，必须先验旧密码，防止别人趁你登录着把密码改掉
  if (cfg.webPass[0] != 0) {
    String op = String(oldPass);
    op.trim();
    if (strcmp(op.c_str(), cfg.webPass) != 0) {
      Log.println("[WEB] 改密码失败：当前密码不对");
      server.send(200, "application/json", "{\"ok\":false,\"msg\":\"当前密码错误\"}");
      return;
    }
  }

  String np = String(newPass);
  np.trim();
  strncpy(cfg.webPass, np.c_str(), sizeof(cfg.webPass) - 1);
  cfg.webPass[sizeof(cfg.webPass) - 1] = 0;
  saveConfig();
  Log.printf("[WEB] 网页密码已更新，长度=%d\n", (int)strlen(cfg.webPass));
  server.send(200, "application/json", "{\"ok\":true}");
}

// 网页上直接切换巴法云 / HA，省得为了进配网去拆机箱按按键。
// 切到哪个模式，就顺带把那个模式要用的参数一起存好。
void handleApiMode() {
  if (!checkAuth()) {
    server.send(401, "application/json", "{\"ok\":false,\"msg\":\"未登录\"}");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"无数据\"}");
    return;
  }
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"格式错误\"}");
    return;
  }

  int mode = doc["mode"] | -1;
  if (mode != 0 && mode != 1) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"模式无效\"}");
    return;
  }

  if (mode == 0) {
    // 切到巴法云，私钥是必填的
    const char* uid = doc["bemfaUid"] | "";
    if (strlen(uid) != 32) {
      server.send(400, "application/json", "{\"ok\":false,\"msg\":\"私钥应为 32 位\"}");
      return;
    }
    strncpy(cfg.bemfaUid, uid, sizeof(cfg.bemfaUid) - 1);
    cfg.bemfaUid[sizeof(cfg.bemfaUid) - 1] = 0;
  } else {
    // 切到 HA，MQTT 服务器地址是必填的
    const char* host = doc["mqttHost"] | "";
    int port = doc["mqttPort"] | 1883;
    if (strlen(host) == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"msg\":\"请填写 MQTT 服务器地址\"}");
      return;
    }
    if (port < 1 || port > 65535) {
      server.send(400, "application/json", "{\"ok\":false,\"msg\":\"端口应在 1-65535\"}");
      return;
    }
    strncpy(cfg.mqttHost, host, sizeof(cfg.mqttHost) - 1);
    cfg.mqttHost[sizeof(cfg.mqttHost) - 1] = 0;
    cfg.mqttPort = port;
  }

  cfg.mode = mode;
  saveConfig();
  Log.printf("[CFG] 切换到 %s 模式，准备重启\n", mode == 0 ? "巴法云" : "HA");
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

// OTA 自己接管，为的是能在写 Flash 之前先验一次密码。
// 不自己写的话，固件库注册的路由没法插鉴权。
bool otaAuthorized = false;

void handleOtaData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaAuthorized = checkAuth();
    if (otaAuthorized) {
      Log.printf("[OTA] 开始接收 %s\n", up.filename.c_str());
      // 按当前剩余的可写空间开一块，官方 OTA 库也是这么算的
      uint32_t maxSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSpace, U_FLASH)) {
        Log.printf("[OTA] 初始化失败 code=%d\n", Update.getError());
      }
    } else {
      Log.println("[OTA] 拒绝：未登录");
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaAuthorized) {
      if (Update.write(up.buf, up.currentSize) != up.currentSize) {
        Log.printf("[OTA] 写入失败 code=%d\n", Update.getError());
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaAuthorized) Update.end(true);
  }
}

void handleOtaDone() {
  if (!otaAuthorized) {
    server.send(401, "text/plain; charset=utf-8", "未登录");
    return;
  }
  if (Update.hasError()) {
    Log.printf("[OTA] 失败 code=%d\n", Update.getError());
    // 用 500 而不是 200：网页那边靠状态码判断成败，
    // 回 200 会让它把"升级失败"也当成成功。
    server.send(500, "text/plain; charset=utf-8", "升级失败");
    return;
  }
  Log.println("[OTA] 成功，准备重启");
  server.send(200, "text/plain; charset=utf-8", "OK");
  delay(500);
  ESP.restart();
}

void setupWebServer() {
  // 必须显式声明要收集 Cookie 头，否则 server.header("Cookie") 永远是空的，
  // 登录验证能过、cookie 也发出去了，但下一个请求读不到，又被打回登录页。
  server.collectHeaders("Cookie");

  server.on("/", handleStatusPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", handleLogout);
  server.on("/api/status", handleApiStatus);
  server.on("/api/log", handleApiLog);
  server.on("/api/restart", handleApiRestart);
  server.on("/api/scan", handleApiScan);
  server.on("/api/scanResult", handleApiScanResult);
  server.on("/api/wifi", HTTP_POST, handleApiWifi);
  server.on("/api/pass", HTTP_POST, handleApiPass);
  server.on("/api/mode", HTTP_POST, handleApiMode);
  server.on("/update", HTTP_POST, handleOtaDone, handleOtaData);
  server.begin();

  // mDNS：同一局域网里用 http://pcboot.local 就能打开，不用记 IP
  if (MDNS.begin("pcboot")) {
    MDNS.addService("http", "tcp", 80);
    Log.printf("[WEB] http://pcboot.local 或 http://%s\n",
               WiFi.localIP().toString().c_str());
  } else {
    Log.printf("[WEB] http://%s\n", WiFi.localIP().toString().c_str());
  }
  Log.printf("[WEB] 网页密码：%s\n", cfg.webPass[0] ? "已设置" : "未设置（谁都能进）");
}

// ============================================================
// 13. 初始化
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Log.println("\n\n=== 电脑开机卡 v2 ===");

  pinMode(PIN_POWER, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  digitalWrite(PIN_POWER, LOW);
  digitalWrite(PIN_RESET, LOW);

  pinMode(PIN_LED, OUTPUT);
  setLed(false);

  pinMode(PIN_STATUS, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  loadConfig();
  setupHaTopics();

  bool needConfig = false;
  if (cfg.wifiSSID[0] == 0) needConfig = true;
  if (cfg.mode == 0 && cfg.bemfaUid[0] == 0) needConfig = true;

  if (needConfig) {
    Log.println("[WiFi] 未配置，进入配网模式");
    startConfigPortal();
    return;
  }

  setLedMode(LED_FAST);
  WiFi.mode(WIFI_STA);
  setupBemfaTopics();          // 先切 STA 模式，MAC 才读得稳
  WiFi.begin(cfg.wifiSSID, cfg.wifiPass);
  Log.printf("[WiFi] 连接 %s", cfg.wifiSSID);

  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) {
    delay(500);
    Log.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Log.println("\n[WiFi] 连接失败，进入配网模式");
    startConfigPortal();
    return;
  }

  Log.printf("\n[WiFi] 已连接 %s\n", WiFi.localIP().toString().c_str());
  setLedMode(LED_FAST);

  setupWebServer();            // 先把网页开起来，后面建主题、连 MQTT 都能边等边看
  ensureTopics();
  connectMqtt();
}

// ============================================================
// 14. 主循环
// ============================================================
void loop() {
  updateLed();
  updatePress();                 // 按键到点就松开，不阻塞主循环
  if (!apMode) checkButton();    // 配网模式下不再响应长按，免得打断配网

  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setLedMode(LED_ON);
    // WiFi 都断了，MQTT 的重连间隔没必要继续翻倍。
    // 重置回默认值，等 WiFi 恢复后能马上重连，而不是干等一分钟。
    mqttRetryGap = 5000;
    // 不阻塞，只限制日志频率，让 LED 和按键继续工作
    static unsigned long lastWarn = 0;
    if (millis() - lastWarn > 5000) {
      lastWarn = millis();
      Log.println("[WiFi] 连接断开，等待重连");
    }
    return;
  }

  if (!mqtt.connected()) {
    // 连不上就慢慢退，别每 5 秒死磕一次。
    // 服务器长时间不可达时，频繁重连会把主循环反复卡住，网页跟着打不开。
    if (millis() - lastMqttRetry > mqttRetryGap) {
      lastMqttRetry = millis();
      connectMqtt();
      if (mqtt.connected()) {
        mqttRetryGap = 5000;              // 连上了，恢复默认间隔
      } else {
        mqttRetryGap *= 2;                // 连不上，间隔翻倍
        if (mqttRetryGap > 60000) mqttRetryGap = 60000;   // 封顶 60 秒
      }
    }
  } else {
    mqtt.loop();
    flushPublish();              // 回调里登记的回发消息，在这里真正发出去
  }

  // 网页服务：处理浏览器请求和 mDNS 广播，都是非阻塞的
  server.handleClient();
  MDNS.update();

  // 心跳日志：每 30 秒一行，用来判断板子到底卡在哪一步。
  // 接上串口看这一行就知道它是不是还活着、MQTT 通不通、按键有没有松开。
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Log.printf("[HB] 运行%lus mqtt=%d press=%d GPIO12=%d GPIO13=%d 状态=%d\n",
                  millis() / 1000,
                  mqtt.connected() ? 1 : 0,
                  pressPin,
                  digitalRead(PIN_POWER),
                  digitalRead(PIN_RESET),
                  lastPowerState ? 1 : 0);
  }

  // 状态检测（每 200ms 一次，去抖）
  if (millis() - lastStatusCheck > 200) {
    lastStatusCheck = millis();
    static int stableCount = 0;
    static bool lastRead = false;
    bool cur = readPowerState();
    if (cur == lastRead) {
      if (stableCount < 5) stableCount++;
      // 状态变了要报；这次启动还没报过也要报一次，保证云端跟实际一致。
      // 这里不能用"启动后第一次"的局部标志：MQTT 连上时已经报过一次，
      // 再报就是重复消息。
      if (stableCount == 5 && (cur != lastPowerState || !stateReported)) {
        lastPowerState = cur;
        stateReported = true;
        reportState(cur);
      }
    } else {
      lastRead = cur;
      stableCount = 0;
    }
  }
}
