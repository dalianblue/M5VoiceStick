# M5VoiceStick 项目计划与交付

## Context

把 M5StickS3 做成语音输入棒：按住 BtnA 录音（≤30s、屏幕显示计时+波形）→ HTTPS 上传到智谱 GLM-ASR-2512 → 返回文字 → 通过自定义 BLE GATT 推给电脑端常驻 Python 伴侣脚本 → 伴侣脚本写剪贴板后模拟 Cmd/Ctrl+V 粘贴到当前焦点。**中文为主、中英混合场景**。

**为什么不用 BLE HID 键盘**：HID 协议只发 scancode，对中文根本性不可行（多音字 IME 发拼音不可靠、Mac Unicode Hex Keyboard 要切输入法、Windows EnableHexNumpad 多数应用不工作）。用户已确认中文为主 + 接受伴侣脚本，因此走纯自定义 GATT + 桌面脚本统一处理所有文字。

**为什么需要电脑端脚本**：BLE 无法直接写入宿主剪贴板或注入 Unicode 文本，必须有一段本地代码接收 BLE 数据后用 OS API 落到屏幕。

**内网友好策略**：伴侣脚本用 PyInstaller 打包成单文件 .exe（Windows）/.app（Mac），U 盘拷贝双击即跑，免装 Python、免 IT 审批。

## 任务清单（全部完成）

| # | 任务 | 状态 | 行数 |
|---|---|---|---|
| 1 | 搭建项目骨架（config.h、partitions.csv、拷 cnfont.h） | ✅ | 56 + 7 |
| 2 | WiFi 配网模块（Captive Portal + NVS 三字段） | ✅ | 244 |
| 3 | 录音模块（M5.Mic 非阻塞 + PSRAM + WAV 头） | ✅ | 138 |
| 4 | ASR HTTPS 上传（multipart + setInsecure + ArduinoJson） | ✅ | 94 |
| 5 | BLE 文本服务（自定义 GATT + MTU 247 + ACK） | ✅ | 113 |
| 6 | UI 渲染（5 页 + 中文字体 + 波形） | ✅ | 265 |
| 7 | 主 .ino 状态机（含 FreeRTOS 上传任务） | ✅ | 265 |
| 8 | 伴侣脚本（voice_stick_rx.py + PyInstaller 打包） | ✅ | 143 |
| 9 | 主 README + 验证 | ✅ | — |

固件总 ~1175 行（不含 cnfont.h 1.88MB 字体表）。

## 关键架构决策

| 决策 | 选择 | 理由 |
|---|---|---|
| 中文输入路径 | 自定义 GATT + 桌面伴侣 | HID 协议对中文根本性不可行；用户接受伴侣脚本 |
| 内网友好 | PyInstaller 单文件 .exe/.app | U 盘拷贝双击即跑，免装 Python、免 IT 审批 |
| 录音 API | M5Unified `record()` + `isRecording()` poll | 单缓冲简单，块间 5-10ms gap 对 ASR 影响微小 |
| 上传 | PSRAM 拼装完整 multipart body + sendRequest | 避开 stream 复杂度，~1MB 一次发 |
| 上传并发 | FreeRTOS task | 主循环继续刷 UI 动画 |
| 凭证存储 | Captive Portal 收集 → NVS | 固件零硬编码敏感信息 |
| TLS | setInsecure() | ESP32 常规做法；升级路径是硬编码根证书 |
| 录音上限 | 30s（960KB PSRAM） | 用户确认 |

## 目标文件结构

```
M5_VoiceStick/
  M5_VoiceStick.ino          # 主入口 + 状态机
  config.h                   # 全局配置（只改这一个）
  wifi_portal.h/.cpp         # Captive Portal 配网 + NVS 存 ssid/pass/apiKey
  mic_recorder.h/.cpp        # M5.Mic 录音 + PSRAM 缓冲 + WAV 头封装
  asr_client.h/.cpp          # HTTPS multipart 上传到 BigModel，setInsecure()
  ble_text_server.h/.cpp     # 自定义 GATT（RX write / TX notify）
  ui_render.h/.cpp           # 待机页/录音页(计时+波形)/上传页/结果页/失败页
  cnfont.h                   # 中文字体子集（596 字符，1.88MB）
  partitions.csv             # 8MB 分区表（沿用 M5StickS3 项目）
  README.md                  # 烧录说明、状态指示、首次使用
  companion/
    voice_stick_rx.py        # bleak + pyperclip + pyautogui，Mac/Win 通用
    requirements.txt
    build_mac.sh             # PyInstaller → .app
    build_win.bat            # PyInstaller → .exe
    README.md                # 伴侣脚本安装/授权/开机自启/故障排查
```

## 状态机

```
BOOT → WIFI_CHECK → IDLE
                     │ 按住 BtnA
                     ▼
                  RECORDING ──(30s 自动截断或松开)──▶ UPLOADING
                     ▲                                │
                     │                                ▼
                  SENT ◀── SENDING ◀── RESULT ◀───(JSON text)
```

## 关键参考代码复用

| 来源 | 复用内容 |
|---|---|
| `~/M5StickS3/time_sync.cpp` | WiFi Captive Portal 模式（WebServer + DNSServer + Preferences），简化去除 NTP/潮汐 |
| `~/M5StickS3/M5StickS3.ino` | M5Unified 初始化、按键长按/短按状态机、屏幕省电三级（亮/暗/灭） |
| `~/M5StickS3/ui_render.cpp` | GFXfont 中文渲染、`#define dsp M5.Display` 模式 |
| `~/M5StickS3/cnfont.h` | 中文字体文件直接拷贝（596 字符子集） |
| `~/M5StickS3/build/.../partitions.csv` | 8MB 分区表原样复用 |

## 验证步骤（实施后跑）

1. **配网**：首次开机进 AP 模式，手机连 `M5VoiceStick-Cfg`，浏览器自动弹 portal，输入 WiFi 密码 + API key，提交后重启
2. **录音显示**：长按 BtnA，屏幕秒数递增 + 波形跳动，对麦克风说"你好世界 hello world"，30 秒内松手
3. **上传**：屏幕显示"上传中…"，~5-10 秒内返回
4. **结果显示**：屏幕显示识别到的文字 + "↑已粘贴" 或 "!BLE 未连"
5. **伴侣脚本源码运行**：Mac 上 `python voice_stick_rx.py`，bleak 连接 M5VoiceStick。光标放到 Notes，StickS3 录一段话，几秒后文字自动粘贴
6. **PyInstaller 打包验证**：`build_mac.sh` 生成 `dist/VoiceStickRX.app`，拷到另一台没装 Python 的 Mac 双击，重复步骤 5
7. **Windows 重复**：`build_win.bat` 验证 `VoiceStickRX.exe` 双击运行 + Ctrl+V 触发
8. **失败路径**：断 WiFi 测上传失败提示；断 BLE 测"BLE 未连"提示

## 已跳过的东西（需要时再加）

- **BLE HID 键盘路径**：用户确认中文为主、放弃 HID；若以后想脱离电脑脚本再加 ESP32-BLE-Keyboard 库做 ASCII fallback
- **OTA 升级 / 固件签名**：手动 USB 烧录够用
- **多语言 ASR 切换**：GLM-ASR-2512 默认中英混合
- **录音编码压缩（mp3/opus）**：WAV 直传更简单，960KB 上传可接受；网络差再加
- **伴侣脚本托盘 UI**：v1 命令行常驻 + 双击 .app/.exe，验证后再加 rumps/menuicon
- **Mac .app 签名/公证**：内网内部分发跳过；外发再加 codesign + notarytool

## 已知限制（运行时可能踩的坑）

- 录音块间 5-10ms gap（单缓冲 + poll `isRecording()`），对 ASR 影响微小。升级双缓冲 ping-pong（`_rec_info[2]`）消除
- 剪贴板竞态：粘贴后 0.4s 恢复原剪贴板，期间用户复制其他内容会被覆盖
- HTTPS setInsecure 跳过证书校验（ESP32 常规做法）。需严格安全时改硬编码根证书
- 无 BLE HID：**电脑端必须跑伴侣脚本**才能输入文字
- M5Unified API 版本差异：`Mic_Class::record()` 在不同版本签名一致，但 `isRecording()` 返回值语义需对照 `Mic_Class.hpp` 当前版本
- ArduinoJson v7 用 `JsonDocument`；v6 需改 `DynamicJsonDocument doc(capacity)`
