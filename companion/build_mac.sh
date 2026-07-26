#!/usr/bin/env bash
# 打包成 .app（PyInstaller）。内网分发可跳过签名/公证。
set -e
cd "$(dirname "$0")"
pip install -q -r requirements.txt pyinstaller
pyinstaller --windowed --name VoiceStickRX \
    --add-data "requirements.txt:." \
    voice_stick_rx.py
echo "打包完成: dist/VoiceStickRX.app"
echo "拷贝到目标 Mac，双击运行（首次需在系统设置 → 隐私 → 辅助功能 / 蓝牙 授权）"
