# M5VoiceStick 桌面伴侣

接收 M5VoiceStick 通过 BLE 发来的识别文字，写入系统剪贴板后模拟 Cmd/Ctrl+V 粘贴到当前焦点。

## 两种运行方式

### 方式 1：直接用 Python 跑（开发/调试）

```bash
pip install -r requirements.txt
python voice_stick_rx.py
```

### 方式 2：打包成单文件可执行（内网分发）

**Mac**（在本机打包，拷到目标 Mac）：
```bash
./build_mac.sh
# 产物：dist/VoiceStickRX.app
```

**Windows**：
```bat
build_win.bat
:: 产物：dist\VoiceStickRX.exe
```

拷贝 `.app` / `.exe` 到目标电脑，双击运行。

## 首次运行授权

### macOS
首次运行 `.app` 需在 **系统设置 → 隐私与安全性** 授权：
- **辅助功能**（模拟键盘必选）
- **蓝牙**（连接 BLE 设备）
- **输入监控**（如果辅助功能路径不工作）

### Windows
- SmartScreen 警告点 **更多信息 → 仍要运行**（未签名）
- 部分杀毒软件可能拦截 pyautogui，加白名单

## 开机自启

### macOS（launchd）
```bash
cat > ~/Library/LaunchAgents/com.voicestick.rx.plist <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.voicestick.rx</string>
  <key>ProgramArguments</key>
  <array>
    <string>/Applications/VoiceStickRX.app/Contents/MacOS/VoiceStickRX</string>
    <string>--hidden</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
</dict></plist>
PLIST
launchctl load ~/Library/LaunchAgents/com.voicestick.rx.plist
```

### Windows
把 `VoiceStickRX.exe` 的快捷方式拖到 `Win+R → shell:startup` 打开的启动文件夹。

## 蓝牙适配器兼容性

本程序仅支持 **BLE（低功耗蓝牙）**，通过 `bleak` 走 Windows WinRT / macOS CoreBluetooth 栈。不兼容经典蓝牙（BR/EDR / SPP）设备。

**不推荐 BCM20702 芯片**（Broadcom，蓝牙 4.0+EDR，常见于老款笔记本和老 USB dongle）：
- Broadcom 已停止维护该芯片的驱动，Windows 通用驱动对 BLE GATT 支持残缺
- 典型表现：经典蓝牙设备（耳机/鼠标）正常，但**扫描到 M5VoiceStick 后无法建立 GATT 连接**，或连接立即断开
- 该问题在 `bleak` 之下（WinRT → 驱动），Python 层无法绕过

推荐芯片（蓝牙 5.0+，兼容性良好）：
- Intel AX200 / AX210
- Realtek 8822BU / 8821CU
- CSR8510（5.0 版本）

### Mini PC 的 USB 供电问题（代码 22）

OptiPlex 7070 Micro、HP EliteDesk 800 G6 DM 等 Mini PC 的前置 USB 端口电流偏低（部分仅 500mA），部分功耗较高（≥200mA）的蓝牙 dongle 无法启动，设备管理器显示 **代码 22（该设备被禁用）** 或设备无法识别。即便接口供电正常，dongle 启动瞬间的浪涌也可能触发过流保护。

建议：
- 优先插 **机身后置 USB 端口**（主板直出，供电更稳）
- 避免 USB 3.0 端口（2.4G 频段易受 USB 3.0 噪声干扰，影响蓝牙），优先 USB 2.0 端口
- 若需扩展，使用**带独立供电的 USB 2.0 集线器**（不依赖主机的总线供电款）

推荐**低功耗、小体积、驱动成熟**的蓝牙 5.0 dongle（实测稳定）：
- TP-Link UB5A（蓝牙 5.0，CSR8510 芯片，约 50 元）—— 功耗低、即插即用
- ASUS USB-BT500（蓝牙 5.0，Realtek，约 80 元）—— 信号稳定
- UGREEN 蓝牙 5.0 适配器（CM390/CM391）—— 兼容性尚可，体积小巧

不推荐：体积大 / 带外置天线 / 标称 "高速 5.3" 的杂牌 dongle，这类通常功耗更高，且驱动来源不明，在 Mini PC 上更易触发代码 22 或驱动冲突。

若已在用 BCM20702，可尝试以下缓解（不保证有效）：
1. 设备管理器 → 蓝牙 → BCM20702 项 → 更新驱动 → 改用 **Microsoft 通用驱动**（替代 Broadcom 自带驱动）
2. 设备管理器 → 蓝牙适配器 → 电源管理 → 取消「允许计算机关闭此设备以节约电源」
3. 重启 `Bluetooth Support Service` 或重新插拔 dongle

最经济的方式是更换一个蓝牙 5.0 的 USB dongle（几十元）。

## 故障排查

| 现象 | 检查 |
|---|---|
| 找不到设备 | M5VoiceStick 是否在 Idle 屏幕（BLE 已广播）；电脑蓝牙开 |
| 连上但不粘贴 | Mac 检查辅助功能权限；Windows 检查焦点是否在文本框 |
| 中文乱码 | 不应该发生，所有传输都是 UTF-8；如乱码看 pyperclip 版本 |
| 粘贴覆盖原剪贴板 | 已实现 0.4s 后恢复，若间隔太短可能丢失 |
