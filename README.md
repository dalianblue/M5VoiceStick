# M5VoiceStick

M5StickS3 语音输入终端：按住 BtnA 说话 → 上传智谱 GLM-ASR-2512 → BLE 推给电脑伴侣脚本 → 自动粘贴到当前光标位置。中文为主、中英混合。

## 工作流

```
按住 A 键 ──▶ 响一声短提示音 ──▶ 录音(≤30s, 屏幕显示秒数+波形)
松手 ──▶ HTTPS 上传 WAV 到 GLM-ASR
返回文本 ──▶ BLE GATT 发给伴侣脚本
伴侣脚本 ──▶ 写剪贴板 → Cmd/Ctrl+V 粘贴到当前焦点
屏幕 ──▶ 显示本次花费 X.XXXX 元（不显示识别文本）
```

## 内网使用场景（核心玩法）

**核心思路**：内网电脑通常禁 U 盘文件读写，但**蓝牙适配器一般是不禁的**——通过外插蓝牙适配器 + M5StickS3 连手机热点，绕开内网限制实现语音输入。

### 流程

1. 准备一个**独立蓝牙适配器**插到内网电脑
2. M5StickS3 配网到自己手机热点（不依赖内网 WiFi）
3. 电脑跑 VoiceStickRX，按住设备 A 键说话，自动粘贴到光标

### 蓝牙适配器兼容性

| 适配器 | 兼容性 | 备注 |
|---|---|---|
| **绿联 CM748** | ✅ 推荐，测试通过 | 适配范围广，首选 |
| 博通 BCM20702 等老蓝牙 4.0 | ❌ 不支持 | 虽然适配的电脑更多，但不支持本客户端 |
| 戴尔 OptiPlex 7070 micro（迷你 PC） | ⚠️ 不工作 | 插上 CM748 后设备管理器报**代码 22** |
| 惠普 EliteDesk 800 G6（迷你 PC） | ⚠️ 不工作 | 同上，代码 22 |
| 稍老的台式机 | ✅ 通常没问题 | —

> 迷你 PC 集成度太高，部分型号 USB/蓝牙资源冲突会报代码 22。优先选**老台式机 + 绿联 CM748** 组合。

### 状态自检（看灯）

M5StickS3 左上角两个指示灯：

| 灯 | 含义 |
|---|---|
| WiFi 灯（绿） | 已连接手机热点 |
| BLE 灯（绿） | 已连接电脑伴侣脚本 |

**两个绿灯同时亮 = 工作正常**，可以直接按 A 键说话。蓝牙**无需手动配对**——脚本会自动扫描连接，灯亮就说明已就绪。

### 费用

- 采用智谱 GLM-ASR 语音转文字模型，**按量计费**（不调用不计费）
- 实测：正常使用频率下，**充值 50 元可用几个月**，折合约 10 元/月
- 详细的 token 计费测算见下方「费用估算」一节

## 硬件

- M5StickS3（ESP32-S3-PICO-1-N8R8，8MB Flash + 8MB PSRAM）
- 内置 SPM1423 PDM 麦克风
- 内置 1.14" LCD 240x135
- BtnA（录音）、BtnB（短按看花费 / 长按重配网或清零）

## 烧录

### Arduino IDE

1. 安装 **M5Unified** 库（库管理器搜索）
2. 安装 **ArduinoJson** （v6/v7 均可）
3. 板管理器添加 M5Stack 板包，选 **M5StickS3**
4. 板选项：
   - Flash Size: 8MB
   - Partition Scheme: `default_8MB`（或自定义用项目根的 `partitions.csv`）
   - Upload Speed: 115200
   - USB Mode: Hardware CDC
   - PSRAM: OPI
5. 打开 `M5_VoiceStick.ino`，编译烧录

### PlatformIO

也可用 PlatformIO，配置参考：
```ini
[env:m5stack-sticks3]
platform = espressif32
board = m5stack-sticks3
framework = arduino
board_build.partitions = partitions.csv
board_build.filesystem = spiffs
lib_deps =
    m5stack/M5Unified
    bblanchon/ArduinoJson
build_flags = -DCORE_DEBUG_LEVEL=0
```

## 首次使用

1. 烧录后上电，屏幕显示 "Setup" 并发出 AP `M5VoiceStick-Cfg`
2. 手机/电脑连这个 WiFi
3. 浏览器自动弹出页面（或访问 `192.168.4.1`）
4. 填 WiFi 名、密码、智谱 API Key，点 Save
5. 设备自动连接 WiFi，回到 Idle 屏

## 日常使用

1. 确保电脑端跑着 **VoiceStickRX**（见 `companion/README.md`）
2. 设备屏幕显示 "按下开始语音识别 →"（黄色箭头指向 BtnA），右上角 W/B 图标都绿色（WiFi+BLE 都通）
3. 光标放到任意输入框
4. 按住设备 A 键 → 响一声短提示音 → 开始说话（≤30s）
5. 松手，等几秒，识别文字自动粘贴到光标

## 状态指示

| 屏幕 | 含义 |
|---|---|
| 按下开始语音识别 → + 绿色 W/B 图标 | Idle，就绪 |
| 顶部 REC + 中部横向波形 + 下方 m:ss | 录音中 |
| 上传中... | HTTPS POST 中 |
| 本次花费 + "已粘贴" | 识别成功，已粘贴到电脑 |
| 本次花费 + "BLE 未连" | 识别成功但 BLE 没连（电脑端没运行伴侣脚本） |
| 总计花费 + "B:返回  长按B:清零" | Total 页，看累计花费 |
| 失败 + HTTP 错误码 | 上传失败（WiFi 断 / API key 错 / 服务异常 / 余额不足）|

## 按键

| 按键 | Idle 页 | Total 页 |
|---|---|---|
| **A** | 按住录音 | 无效（先按 B 回 Idle） |
| **B 短按** | 进入 Total 页（看累计花费） | 返回 Idle |
| **B 长按 3s** | 重配网（清 WiFi 凭证） | 清零累计花费 |

## Result 页（识别后）

每次识别完成显示**本次花费**（4 位小数，元），不再显示识别文本——文本已通过 BLE 直接粘贴到电脑，屏上不需要再读一遍。

```
┌──────────────────────────┐
│ 本次花费（元）             │  ← 单位在 label
│                          │
│       0.0015             │  ← 大号数字居中
│                          │
│       BLE 未连            │  ← 仅未粘贴时显示
└──────────────────────────┘
```

## Total 页（按 B 进入）

```
┌──────────────────────────┐
│ 总计花费 (元)              │  ← 单位在 label
│                          │
│       1.2345             │  ← 大号数字居中
│                          │
│    B:返回 长按清零         │  ← 居中
└──────────────────────────┘
```

累计金额持久化在 NVS，重启不丢。**长按 B 3s 清零**（伴随短促提示音）。

## 蓝牙连接说明

### 不需要手动配对

本设备用 **BLE（低功耗蓝牙）**，不是经典蓝牙。**不要**去系统设置里搜设备配对——BLE 设备在那里通常也搜不到，是正常的。

`companion/voice_stick_rx.py` 用 `bleak` 库自动扫描设备名 `M5VoiceStick` 并直连，跳过系统配对流程。

### macOS 首次运行必须授权

跑脚本的进程（终端 / Python / 打包的 `.app`）需要：

- **系统设置 → 隐私与安全性 → 蓝牙** → 勾上对应进程
- **系统设置 → 隐私与安全性 → 辅助功能** → 同上（模拟 Cmd+V 粘贴）

首次运行会弹授权窗口。漏掉的话脚本要么扫不到设备，要么识别成功但不粘贴。

### 刷固件后连不上：`Peer removed pairing information`

**症状**：日志出现
```
[conn] 异常: failed to connect: Error Domain=CBErrorDomain Code=14
"Peer removed pairing information"
```

**原因**：macOS 之前跟设备建过 bond 记录（底层自动协商，跟系统设置里的"配对"不是一回事），但 ESP32 刷固件 / 清 NVS 后丢失了对应密钥，对不上号。

**解决**（按代价从低到高，先试 1）：

1. **重启 Mac 蓝牙**：系统设置 → 蓝牙 → 关 → 等 10 秒 → 开。90% 情况够用
2. **重启 Mac**：清掉蓝牙运行时状态
3. **清蓝牙缓存**（影响其他蓝牙设备，最后手段）：
   ```bash
   sudo killall bluetoothd
   sudo rm /Library/Preferences/com.apple.Bluetooth.plist
   sudo reboot
   ```
   重启后鼠标/键盘等已配对设备需要重连

### Windows

- SmartScreen 警告点 **更多信息 → 仍要运行**（未签名）
- 部分杀毒软件拦截 `pyautogui`，加白名单

### 迷你 PC 插蓝牙适配器报「代码 22」

代码 22 = `CM_PROB_DISABLED`（**设备被禁用**）。戴尔 OptiPlex 7070 micro / 惠普 EliteDesk 800 G6 等迷你 PC 上常见。

**关键现象**：同一台机器插 BCM20702（BT 4.0）能用，插 CM748（BT 5.x）就报代码 22 → 排除组策略锁（否则两个都禁），**真凶是与板载 BT 5.x 模块的协议/资源冲突**——Windows 检测到冲突，把后插入的 USB 适配器自动禁用。

**解法**（按命中率排序）：

1. **BIOS 禁用板载蓝牙**（最有效）：开机进 BIOS → 找 `Wireless`/`Bluetooth`/`Onboard BT` 选项 → 关闭 → 保存重启 → 插 CM748。戴尔 BIOS 路径：`System Configuration` → `Wireless Device`；惠普：`Advanced` → `Built-in Device Options`
2. **设备管理器里禁用板载 BT**：找到「Intel Wireless Bluetooth」/「Realtek Bluetooth Adapter」等板载设备 → 禁用 → 只留 CM748
3. **手动启用 CM748**：设备管理器里找带 ↓ 箭头的 CM748 → 右键「启用设备」。不行就「卸载设备」→ 拔下重插让 Windows 重装
4. **更新驱动**：从绿联官网下 CM748 专用驱动；或在设备管理器里「浏览我的电脑以查找驱动程序」→ 从可用驱动列表试不同厂商版本（CM748 多为 CSR/Realtek 方案）
5. **换后置 USB 口**：避开前面板端口（次要，CM748 工作电流仅 100mA，供电通常不是瓶颈）

> 实测经验：方法 1（BIOS 关板载 BT）命中率最高，迷你 PC 板载模块和 USB BT 5.x 适配器同频/同协议冲突是代码 22 的主要原因。

## 文件结构

```
M5_VoiceStick/
  M5_VoiceStick.ino       主程序 + 状态机
  config.h                全局配置（只改这个文件）
  partitions.csv          8MB 分区表
  cnfont.h                中文字体子集（auto-generated by rebuild_cnfont.py）
  rebuild_cnfont.py       字体子集重建脚本（改文案后重跑）
  wifi_portal.{h,cpp}     WiFi Captive Portal 配网 + NVS 花费累计
  mic_recorder.{h,cpp}    PDM 麦克风录音 + WAV 封装
  asr_client.{h,cpp}      智谱 ASR HTTPS 上传 + token 用量
  ble_text_server.{h,cpp} 自定义 GATT 文本通道
  ui_render.{h,cpp}       屏幕 UI（待机/录音/上传/结果/失败/总计）
  companion/              电脑端伴侣脚本（Mac/Windows）
```

## 已知限制

- **录音块间 5-10ms gap**：单缓冲 + poll `isRecording()`，对 ASR 影响微小。要无缝切换双缓冲再升级。
- **剪贴板竞态**：粘贴后 0.4s 恢复原剪贴板。期间用户复制其他内容会被覆盖。
- **HTTPS setInsecure**：跳过证书校验（ESP32 上的常规做法）。需严格安全时改硬编码根证书。
- **无 BLE HID**：放弃 HID 键盘方案，因此**电脑端必须跑伴侣脚本**才能输入文字。
- **字体子集有限**：cnfont.h 只覆盖 UI + ASR 常见标点（151 字符 + ASCII）。Result 页已不显示文本，影响小；改文案必须重跑 `rebuild_cnfont.py`。
- **花费清零不可撤销**：清零前想留底，可从串口日志拼回。

## 配置

改 `config.h` 调整：
- 录音时长上限 `RECORD_MAX_SECONDS`（影响 PSRAM 缓冲大小）
- BLE UUID（如需多设备共存）
- 屏幕旋转方向 `SCREEN_ROTATION`、亮度 `BRIGHTNESS_BOOT`
- 省电超时 `POWER_SCREEN_DIM_MS / OFF_MS`
- CPU 频率 `POWER_CPU_FREQ_MHZ`

## 费用估算（智谱 GLM-ASR-2512）

按 **token 计费**（非"元/秒"营销口径）。2026-07 实测：2.64s 中文音频 ≈ 95 prompt_tokens，约 **0.00152 元/次**，即 **~0.000576 元/秒**（约网站标称 0.0002 元/秒 的 3 倍）。

50 元预算约可录 **24 小时纯音频**：

| 强度 | 单次时长 | 每天次数 | 50 元可用 |
|---|---|---|---|
| 轻度 | 5s | 10 次 | ~4.8 年 |
| 中度 | 8s | 30 次 | ~1 年 |
| 重度 | 10s | 50 次 | ~5.8 个月 |

> 费率可能随官方调价变化，以 [open.bigmodel.cn](https://open.bigmodel.cn) 控制台公布为准。余额耗尽时屏幕显示 `HTTP 429`（错误码 `1113`）。

## 故障排查

| 屏幕现象 | 原因与处理 |
|---|---|
| `HTTP 429` + 错误码 `1113` | **余额不足**，去控制台充值 |
| `HTTP 429`（其他） | 请求频率超限，降低按按钮频率 |
| `HTTP 401/403` | API Key 错误，**Idle 页长按 BtnB 3s 重配网** |
| 识别成功但不粘贴 | 伴侣脚本未运行 / 没给辅助功能权限 |
| 找不到设备 | 设备屏幕是否在 Idle、电脑蓝牙是否开 |
| BLE 连接报 `Peer removed pairing information` | 见上面「蓝牙连接说明」一节 |
| 屏幕显示方框/乱码 | UI 文案改了没重跑 `rebuild_cnfont.py` |

## 开发踩坑记录

### 1. 中文字体子集必须包含所有 UI 字符

项目用 `rebuild_cnfont.py` 从 M5StickS3 的完整字体 `cnfont_full_uint32.h` 提取子集生成 `cnfont.h`（默认只 152 字符）。**改任何 UI 文案后必须重跑 `rebuild_cnfont.py`**，否则缺字会显示成方框。

```bash
python3 rebuild_cnfont.py
```

UI 字符集在脚本顶部的 `UI_TEXT` 变量里集中维护，单一来源。子集已覆盖：UI 中文 + ASCII + 数字 + ASR 输出常见标点（`，。！？、；""''《》【】…`）。

**注意**：源字体 `cnfont_full_uint32.h` 本身没有全角括号 `（）`，只有半角 `()` 和书名号 `《》`。要用括号就用半角 `(元)`，别用全角否则显示方框。

### 2. 跨字体混排永远对不齐

M5GFX/Adafruit_GFX 里 `font 0`（默认 5x8 ASCII）和 `cnfont`（16pt 中文）的 glyph 高度、yOffset、baseline 都不同。手算 offset 让两者底对齐/中心对齐永远差几像素。

**走过的弯路**：让数字和「元」都用 cnfont 同 textSize 渲染——对齐是对齐了，但 cnfont 的 ASCII 数字 glyph 也按 CJK 宽度算，textSize 2 时数字巨大、「元」被挤到看不见。

**最终解法**：**把单位挪进顶部 label**（"本次花费（元）"），中央只放大号数字（font 0 textSize 3）。同一行只用一种字体一种 size，没有对齐问题。

### 3. UTF-8 字符串不能按字节遍历打印

错误示例：
```cpp
for (size_t i = 0; i < text.length(); i++) {
    LCD.print(text[i]);   // UTF-8 字节，中文每字节都不是合法字符
}
```
中文 UTF-8 是 3 字节，按字节 `print` 渲染出乱码 + 列宽错乱。

**解法**：要么 `LCD.print(text)` 让 GFX 内部 UTF-8 解码，要么用 `setTextWrap(true, false)` + `print` 整串自动换行。

### 4. ESP32 BLE 不需要系统配对

BLE（低功耗蓝牙）≠ 经典蓝牙。**不要**去 macOS 系统设置里搜设备配对——BLE 设备在那里通常搜不到，是正常的。`bleak`（Python）/ CoreBluetooth 直接扫描 + 连接，跳过系统配对。

但 BLE 底层有 bonding（密钥持久化）：ESP32 刷固件 / 清 NVS 后，主机端的 bond 记录会变成 stale，报 `Peer removed pairing information`。解决：重启 Mac 蓝牙，或删 `/Library/Preferences/com.apple.Bluetooth.plist`（影响其他蓝牙设备）。

### 5. API 凭证永不进源码

智谱 API Key 通过 Captive Portal 由用户输入，存 ESP32 NVS（`Preferences` 库），运行时 `getApiKey()` 读取。源码、文档、git 历史里都没有真实 Key。

`.gitignore` 里 `config.md` / `*.secret` / `*.key` 兜底，防止开发期临时文件误提交。

### 6. 真实计费 ≠ 营销口径

智谱官网标「~0.0002 元/秒」，实测对不上。**真实计费是 token-based**：API 返回 `usage.prompt_tokens`，按 **16 元 / 百万 tokens** 计价。

固件按 token 数算花费（`formatCost()` 里 `tokens * 16 / 1,000,000` 元），永远跟账单一致。若官网调价，改 `ui_render.cpp:formatCost` 里的乘数。

### 7. M5StickS3 没有内置喇叭

M5StickS3 主板 **没有**内置蜂鸣器/喇叭，发声必须靠 HAT-SPK2 等外接扩展。注释里写「内置喇叭」是错的。

### 8. M5Unified Speaker 默认音量是 0

`M5.Speaker.begin()` 已被 `M5.begin()` 内部调用，无需显式调用。但**默认音量是 0**，必须 `M5.Speaker.setVolume(128)` 才能听见。参考：

```cpp
M5.begin(cfg);
M5.Speaker.setVolume(128);        // 0-255，64 太小，128 适中，255 最大
M5.Speaker.tone(800, 80);         // 非阻塞，freq Hz, duration ms
```

短音 80ms 即可，按键反馈用 1500Hz 比 800Hz 更清脆。

### 9. ESP32 Arduino Core 3.x BLE API 变更

`BLECharacteristicCallbacks::onWrite`、`BLEServerCallbacks::onConnect` 在 Core 3.x 移除了 `esp_ble_gatts_cb_param_t*` 参数。`BLECharacteristic::getValue()` 返回 `String`（不再是 `std::string`），用 `.length()` 而非 `.size()`。

### 10. `dsp` 不能作为宏名

`ui_render.h` 早期用 `#define dsp M5.Display`，但 M5Unified 内部 `addDisplay(M5GFX& dsp)` 把 `dsp` 用作参数名，宏替换后整个头文件崩了。改成 `#define LCD M5.Display` 解决。

**通用经验**：宏名取短词（`dsp` / `spk` / `dev`）容易撞库，用更具体的名字（`LCD` / `SPK`）更安全。

### 11. `setTextSize` 是全局状态，调完不复位会污染后续渲染

`drawCostCentered` 内部设了 `textSize(3)` 画大号数字，函数返回后 textSize 仍是 3。后续打印底部 hint 时没显式 reset，结果整段文字以 3 倍尺寸渲染——宽度爆炸，部分字符挤出屏幕。

**解法**：任何「临时改 textSize / 字体 / 颜色」的函数，返回前必须复位。或者下游每次打印前显式 `setTextSize(1)`，别假设上游状态。

```cpp
LCD.setTextSize(3);    // 局部放大
LCD.print(bigNumber);
LCD.setTextSize(1);    // 复位
```

Adafruit_GFX/M5GFX 的所有 `setText*` 都是状态机——`setTextColor`、`setTextSize`、`setTextWrap`、`setFont` 改了就一直生效，跟 `fillScreen` 那种即时操作不一样。

### 12. ESP32 I2S mic / speaker 抢资源，tone 后必须 delay

`M5.Speaker.tone(freq, dur)` 是**非阻塞**的——立刻返回，后台任务慢慢播。但如果 tone 之后立刻 `M5.Mic.start()`，mic 的 I2S RX 配置会打断 speaker 的 TX 输出，提示音被吞掉。

**Bug 表现**：按下 A 键听不到提示音。

**解法**：tone 和 mic 启动之间加 delay，让提示音播完再开始录：

```cpp
M5.Speaker.tone(1500, 100);
delay(120);              // 等 tone 播完
Mic.start();             // 再开 mic
```

> 80-120ms 的延迟对用户体验完全可接受——听到「嘀」声后才开始说话反而更自然。

### 13. M5Unified `config_t` 没有 `cfg.mic` 字段

想当然写了：
```cpp
cfg.mic.mic_enabled = true;       // ❌ 编译错误
cfg.mic.sample_rate = MIC_SAMPLE_RATE;
```

`M5Unified::config_t` 里**没有 mic 子结构**。M5StickS3 内置 PDM mic 默认已启用，采样率 / 位深在每次 `M5.Mic.record(buf, samples, sample_rate)` 时传参指定，不在 `M5.config()` 里。

### 14. cnfont 16pt 的 yAdvance 是 37，不是 18

`cnfont_subset16pt8b`（16pt LXGWWenKaiLite 子集）的 `yAdvance` 是 **37**（不是按字号 16-18 估算）。但单行文本只看 glyph 实际高度（`h + yo`，比如「元」h=24 yo=-21 → 占 25px），yAdvance 只在 `\n` 换行时影响行距。

调试 UI 时别拿 yAdvance 当字符高度估——用 `textWidth` + 实际 glyph metrics（看 `cnfont.h` 末尾的 `// 0xXXXX 'X'` 注释行）。

### 15. 字体子集生成器的「空 bitmap 占位符 advance」陷阱

`rebuild_cnfont.py` 早期版本：源字体里 bitmap 为空的字符（空格、不可见字符）走占位符分支，**硬编码 `advance=31`**（CJK 全宽）。结果：

- 半角空格在子集里变成 31px 宽（应该是 11px）→ 文字中间莫名出现大空隙
- 子集里没显式声明的 ASCII（半角冒号 `:` 等）也走占位符 → 渲染成 31px 宽的不可见块，看起来像「字符消失了」

**修法**：占位符分支分两种情况——

```python
if c in KEEP:
    # 在保留集但 bitmap 为空：保留源字体的 advance（如空格）
    new_gl.append((0, 1, 1, adv, 0, 0))
else:
    # 真正不在保留集：用默认 advance
    new_gl.append((0, 1, 1, 31, 0, 0))
```

**通用经验**：自己写字体子集生成器时，bitmap 和 advance 是两件事——bitmap 决定能不能看见，advance 决定占多宽。空 bitmap 不代表 advance 也要丢。
