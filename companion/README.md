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

## 故障排查

| 现象 | 检查 |
|---|---|
| 找不到设备 | M5VoiceStick 是否在 Idle 屏幕（BLE 已广播）；电脑蓝牙开 |
| 连上但不粘贴 | Mac 检查辅助功能权限；Windows 检查焦点是否在文本框 |
| 中文乱码 | 不应该发生，所有传输都是 UTF-8；如乱码看 pyperclip 版本 |
| 粘贴覆盖原剪贴板 | 已实现 0.4s 后恢复，若间隔太短可能丢失 |
