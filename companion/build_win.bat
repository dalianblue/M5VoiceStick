@echo off
REM 打包成 .exe（PyInstaller）
cd /d "%~dp0"
pip install -q -r requirements.txt pyinstaller
pyinstaller --noconsole --name VoiceStickRX voice_stick_rx.py
echo 打包完成: dist\VoiceStickRX.exe
echo 拷贝到目标 Windows，双击运行（SmartScreen 警告选"仍要运行"）
pause
