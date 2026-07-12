#!/usr/bin/env python3
"""M5VoiceStick 桌面伴侣脚本。

流程：
  1. 扫描并连接名为 M5VoiceStick 的 BLE 设备
  2. 订阅 TX 特征（ESP32 → 本机）
  3. 收到完整帧（以 \\n\\n 结尾）后：
     - 写入系统剪贴板
     - 模拟 Cmd+V (macOS) / Ctrl+V (Windows) 粘贴到当前焦点
     - 通过 RX 特征回写 "OK\\n" 确认
  4. 断线自动重连
"""
import asyncio
import platform
import sys
import time

from bleak import BleakClient, BleakScanner
import pyperclip
import pyautogui

# 与固件 config.h 对齐
DEVICE_NAME   = "M5VoiceStick"
SERVICE_UUID  = "0000ff01-0000-1000-8000-00805f9b34fb"
RX_CHAR_UUID  = "0000ff02-0000-1000-8000-00805f9b34fb"   # 本机 → ESP32 (write)
TX_CHAR_UUID  = "0000ff03-0000-1000-8000-00805f9b34fb"   # ESP32 → 本机 (notify)
MSG_TERMINATOR = b"\n\n"
ACK           = b"OK\n"

IS_MAC = platform.system() == "Darwin"


def paste():
    """模拟粘贴。Mac 失败时回退 AppleScript。"""
    if IS_MAC:
        try:
            pyautogui.hotkey('command', 'v')
            return
        except Exception as e:
            print(f"[paste] pyautogui 失败，回退 AppleScript: {e}")
            import subprocess
            subprocess.run(['osascript', '-e',
                'tell application "System Events" to keystroke "v" using command down'
            ], check=False)
    else:
        pyautogui.hotkey('ctrl', 'v')


class VoiceStickClient:
    def __init__(self, ble_client: BleakClient):
        self.client = ble_client
        self.buf = bytearray()
        self.last_paste = 0.0

    def on_notify(self, _sender, data: bytearray):
        self.buf.extend(data)
        while MSG_TERMINATOR in self.buf:
            idx = self.buf.index(MSG_TERMINATOR)
            frame = self.buf[:idx]
            del self.buf[:idx + len(MSG_TERMINATOR)]
            asyncio.create_task(self.handle_frame(frame))

    async def handle_frame(self, frame: bytes):
        text = frame.decode('utf-8', errors='replace').strip()
        if not text:
            return
        now = time.monotonic()
        if now - self.last_paste < 0.1:
            await asyncio.sleep(0.1)
        self.last_paste = time.monotonic()

        print(f"[recv] {text[:80]}{'...' if len(text) > 80 else ''}")

        try:
            old = pyperclip.paste()
        except Exception:
            old = ""
        try:
            pyperclip.copy(text)
        except Exception as e:
            print(f"[clip] 写剪贴板失败: {e}")
            return
        await asyncio.sleep(0.05)
        try:
            paste()
        except Exception as e:
            print(f"[paste] 失败: {e}")
            return

        # 发送 ACK 给 ESP32
        try:
            await self.client.write_gatt_char(RX_CHAR_UUID, ACK, response=False)
        except Exception as e:
            print(f"[ack] 失败: {e}")

        # 0.4s 后恢复原剪贴板
        await asyncio.sleep(0.4)
        try:
            pyperclip.copy(old)
        except Exception:
            pass


async def run_one():
    print(f"[scan] 搜索 {DEVICE_NAME} ...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if not device:
        print("[scan] 未找到，5s 后重试")
        return
    print(f"[conn] 连接 {device.address}")
    try:
        async with BleakClient(device, timeout=15.0) as client:
            print("[conn] 已连接，订阅 TX")
            vsc = VoiceStickClient(client)
            await client.start_notify(TX_CHAR_UUID, vsc.on_notify)
            while client.is_connected:
                await asyncio.sleep(0.3)
    except Exception as e:
        print(f"[conn] 异常: {e}")
    print("[conn] 断开，3s 后重试")


async def main():
    pyautogui.FAILSAFE = False
    pyautogui.PAUSE = 0
    print(f"[boot] M5VoiceStick RX @ {platform.system()}")
    while True:
        try:
            await run_one()
        except KeyboardInterrupt:
            print("[exit] 用户中断")
            return
        await asyncio.sleep(3.0)


if __name__ == "__main__":
    # ponytail: --hidden 模式抑制 stdout，PyInstaller windowed 用
    if "--hidden" in sys.argv:
        import contextlib, io
        with contextlib.redirect_stdout(io.StringIO()):
            asyncio.run(main())
    else:
        asyncio.run(main())
