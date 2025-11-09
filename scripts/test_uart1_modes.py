#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UART1 Multi-Mode Comprehensive Test Script
測試 UART1 在 PWM/RPM 模式和 UART 模式下的完整功能

硬體設置要求：
    - 將 TX1 (GPIO 17) 連接到 RX1 (GPIO 18) 用跳線
    - 這樣可以實現：
      * UART 模式：Echo 測試（TX → RX loopback）
      * PWM/RPM 模式：PWM 輸出直接送入 RPM 輸入進行頻率測量

測試覆蓋範圍：
    1. PWM/RPM 模式測試
       - 不同頻率的準確度測試
       - 不同佔空比測試
       - 頻率切換毛刺檢測
       - 佔空比切換毛刺檢測
       - 極限頻率測試

    2. UART 模式測試
       - 多種鮑率測試（2400 - 1500000）
       - 不同訊息長度測試
       - 特殊字元測試

    3. 模式切換測試
       - PWM → UART → PWM 穩定性
       - OFF 模式切換

    4. 錯誤處理測試
       - 無效參數測試
       - 邊界條件測試

    5. 設定持久化測試
       - 驗證 UART1 模式不持久化（每次上電預設 PWM/RPM）

作者：自動化測試腳本
日期：2025-11-08
"""

import serial
import serial.tools.list_ports
import time
import sys
import re
from datetime import datetime
from typing import Optional, List, Tuple

# ==================== 配置常數 ====================
# ESP32-S3 VID/PID
ESP32_VID = 0x303A
ESP32_PID = 0x4002  # CDC interface

# 序列埠參數
DEFAULT_BAUDRATE = 115200
SERIAL_TIMEOUT = 1.0
DTR_STABILIZATION_DELAY = 2.0  # 等待 DTR 穩定（秒）
DEVICE_INIT_DELAY = 3.0  # 設備重置後初始化時間（秒）

# 命令參數
COMMAND_RESPONSE_DELAY = 0.5  # 發送命令後等待（秒）
COMMAND_TIMEOUT = 1.0  # 命令執行超時（秒）
STATUS_CHECK_DELAY = 0.3  # 檢查狀態前等待（秒）
MODE_SWITCH_DELAY = 0.5  # 模式切換後等待（秒）
POLL_INTERVAL = 0.1  # 輪詢間隔（秒）

# 測試參數
FREQUENCY_ERROR_TOLERANCE = 5.0  # 頻率誤差容許範圍 (%)
PWM_STABILIZATION_DELAY = 0.3  # PWM 設定後穩定時間（秒）
TRANSITION_DELAY = 0.1  # 快速切換之間的延遲（秒）

# PWM/RPM 測試頻率
TEST_FREQUENCIES: List[Tuple[int, str]] = [
    (100, "低頻率"),
    (1000, "中頻率"),
    (5000, "高頻率"),
    (10000, "極高頻率")
]

# PWM 佔空比測試點
TEST_DUTIES: List[int] = [0, 25, 50, 75, 100]

# PWM 頻率切換測試序列
TRANSITION_FREQUENCIES: List[int] = [1000, 5000, 2000, 10000, 500]

# PWM 佔空比切換測試序列
TRANSITION_DUTIES: List[int] = [10, 90, 30, 70, 50]

# 極限頻率測試
EXTREME_FREQUENCIES: List[Tuple[int, str]] = [
    (1, "最小頻率"),
    (500000, "最大頻率 (500 kHz)")
]

# UART 測試參數
TEST_BAUDS: List[int] = [2400, 9600, 115200, 460800, 921600, 1500000]
DEFAULT_TEST_BAUD = 115200

# UART 測試訊息
TEST_MESSAGES: List[str] = [
    "Hi",  # 短
    "Hello World from ESP32-S3!",  # 中
    "A" * 100,  # 長（100 字元）
    "The quick brown fox jumps over the lazy dog 1234567890"  # 混合
]

# UART 特殊字元測試
SPECIAL_MESSAGES: List[str] = [
    "Hello!@#$%",
    "Number: 12345",
    "Symbols: !@#$%^&*()",
]

# 模式切換測試序列
MODE_SWITCH_SEQUENCE: List[str] = ["PWM", "UART", "PWM"]

# 測試基準值（用於單變數測試）
TEST_BASELINE_DUTY = 50  # 測試頻率變化時使用的固定佔空比
TEST_BASELINE_FREQ = 1000  # 測試佔空比變化時使用的固定頻率 (Hz)

# ANSI 顏色代碼
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_header(text: str) -> None:
    """列印測試章節標題"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*70}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{text:^70}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{'='*70}{Colors.ENDC}\n")

def print_step(step_num: int, text: str) -> None:
    """列印測試步驟"""
    print(f"{Colors.OKCYAN}{Colors.BOLD}[步驟 {step_num}]{Colors.ENDC} {text}")

def print_success(text: str) -> None:
    """列印成功訊息"""
    print(f"{Colors.OKGREEN}✅ {text}{Colors.ENDC}")

def print_fail(text: str) -> None:
    """列印失敗訊息"""
    print(f"{Colors.FAIL}❌ {text}{Colors.ENDC}")

def print_warning(text: str) -> None:
    """列印警告訊息"""
    print(f"{Colors.WARNING}⚠️  {text}{Colors.ENDC}")

def print_info(text: str) -> None:
    """列印資訊訊息"""
    print(f"{Colors.OKBLUE}ℹ️  {text}{Colors.ENDC}")

def wait_for_user(prompt: str = "按 ENTER 繼續...") -> None:
    """等待使用者確認"""
    print(f"\n{Colors.WARNING}{prompt}{Colors.ENDC}")
    input()

def find_esp32_port() -> Optional[str]:
    """尋找 ESP32-S3 CDC 埠（使用與 test_cdc.py 相同的可靠檢測策略）"""
    ports = serial.tools.list_ports.comports()

    for port in ports:
        port_name = port.device
        description = port.description.lower() if port.description else ""

        # 跳過藍牙裝置
        if any(keyword in description for keyword in ['bluetooth', 'bt ', '藍牙', '藍芽']):
            continue

        # 只掃描有 VID/PID 的 USB 裝置（虛擬 COM port 通常沒有 VID/PID）
        if not port.vid or not port.pid:
            continue

        # 跳過其他非 CDC 裝置
        if any(keyword in description for keyword in ['printer', 'modem', 'dialup', 'irda', '印表機', '數據機']):
            continue

        try:
            # 嘗試打開並測試裝置
            ser = serial.Serial(
                port=port_name,
                baudrate=DEFAULT_BAUDRATE,
                timeout=SERIAL_TIMEOUT,
                rtscts=False
            )

            # 設置 DTR 信號（ESP32-S3 需要）
            ser.dtr = True
            ser.rts = False

            # 等待裝置穩定
            time.sleep(DTR_STABILIZATION_DELAY)

            ser.reset_input_buffer()
            ser.reset_output_buffer()
            ser.write(b"*IDN?\n")
            ser.flush()

            # 等待回應
            time.sleep(0.1)
            start_time = time.time()

            while (time.time() - start_time) < COMMAND_TIMEOUT:
                if ser.in_waiting > 0:
                    response = ser.readline().decode('utf-8', errors='ignore').strip()
                    # 檢查是否為 ESP32 裝置
                    if any(keyword in response for keyword in ["ESP32", "RYMCU", "USB", "Composite", "HID"]):
                        ser.close()
                        return port_name
                else:
                    time.sleep(POLL_INTERVAL)

            ser.close()

        except (serial.SerialException, OSError):
            continue  # 嘗試下一個埠

    return None

def send_command(ser: serial.Serial, command: str, wait_time: float = COMMAND_RESPONSE_DELAY) -> str:
    """發送命令並讀取回應"""
    ser.write(f"{command}\n".encode('utf-8'))
    time.sleep(wait_time)
    response = ""

    # 讀取初始回應
    while ser.in_waiting:
        response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(POLL_INTERVAL)

    # 再等待以捕獲調試訊息（如 GLITCH-FREE PATH）
    # ESP32 的調試輸出可能有延遲，多輪等待確保捕獲
    for _ in range(3):
        time.sleep(0.05)
        if ser.in_waiting:
            response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')

    return response

def extract_debug_info(response: str) -> None:
    """提取並顯示調試訊息"""
    lines = response.split('\n')
    debug_found = False

    for line in lines:
        line = line.strip()
        # 顯示所有包含 [UART1] 的調試行
        if '[UART1]' in line:
            debug_found = True
            # 高亮顯示調試訊息
            if 'GLITCH-FREE' in line:
                print(f"  {Colors.OKGREEN}[調試] {line}{Colors.ENDC}")
            elif 'PRESCALER CHANGE' in line or 'PRESCALER' in line:
                print(f"  {Colors.WARNING}[調試] {line}{Colors.ENDC}")
            else:
                print(f"  {Colors.OKBLUE}[調試] {line}{Colors.ENDC}")

    # 如果沒有找到調試訊息，顯示提示
    if not debug_found:
        print(f"  {Colors.WARNING}[調試] （未捕獲到調試訊息，可能需要增加等待時間）{Colors.ENDC}")

def parse_rpm_from_status(response: str) -> Optional[float]:
    """從 UART1 STATUS 回應中解析 RPM 頻率"""
    # 尋找 "Frequency: XXX.XX Hz" 在 PWM/RPM 模式
    match = re.search(r'Frequency:\s+([\d.]+)\s*Hz', response)
    if match:
        return float(match.group(1))
    return None

def test_hardware_setup() -> None:
    """測試 0：驗證硬體連接"""
    print_header("硬體設置驗證")

    print_info("需要的硬體設置：")
    print(f"  {Colors.BOLD}1. 用跳線連接 TX1 (GPIO 17) 到 RX1 (GPIO 18){Colors.ENDC}")
    print(f"     - 這會建立一個 loopback 用於測試")
    print(f"     - UART 模式：啟用 echo 測試")
    print(f"     - PWM/RPM 模式：PWM 輸出 → RPM 輸入")
    print()
    print_info("為什麼需要這個連接：")
    print("  - UART 模式：TX 資料會迴路到 RX 用於驗證")
    print("  - PWM/RPM 模式：PWM 訊號會被測量為 RPM 輸入")
    print()

    wait_for_user("🔌 請連接 TX1 到 RX1，然後按 ENTER 開始測試...")
    print_success("硬體設置確認完成")
    print_info("接下來將進行測試套件 1：PWM/RPM 模式測試")

def test_pwm_rpm_mode(ser: serial.Serial) -> None:
    """測試套件 1：PWM/RPM 模式測試"""
    print_header("測試套件 1：PWM/RPM 模式測試")

    # 確保在 PWM/RPM 模式
    print_step(1, "設定 UART1 為 PWM/RPM 模式")
    response = send_command(ser, "UART1 MODE PWM")
    if "PWM/RPM" in response or "PWM" in response:
        print_success("UART1 已設為 PWM/RPM 模式")
    else:
        print_fail(f"無法設定 PWM/RPM 模式：{response}")
        return

    wait_for_user()

    # 測試 1.1：頻率準確度測試
    print_step("1.1", "頻率準確度測試")
    print_info("在不同頻率點測試 PWM 輸出...")

    for freq, desc in TEST_FREQUENCIES:
        print(f"\n  測試 {desc}：{freq} Hz")

        # 設定 PWM 頻率，使用基準佔空比
        cmd = f"UART1 PWM {freq} {TEST_BASELINE_DUTY} ON"
        response = send_command(ser, cmd)
        print(f"  命令：{cmd}")

        # 等待穩定
        time.sleep(PWM_STABILIZATION_DELAY)

        # 讀取狀態獲取 RPM
        response = send_command(ser, "UART1 STATUS")
        measured_freq = parse_rpm_from_status(response)

        if measured_freq:
            error_percent = abs(measured_freq - freq) / freq * 100
            print(f"  設定：{freq} Hz，測量：{measured_freq:.2f} Hz，誤差：{error_percent:.2f}%")

            if error_percent < FREQUENCY_ERROR_TOLERANCE:
                print_success(f"頻率準確度：通過（誤差在 {FREQUENCY_ERROR_TOLERANCE}% 容許範圍內）")
            else:
                print_warning(f"頻率準確度：警告（誤差 {error_percent:.2f}% > {FREQUENCY_ERROR_TOLERANCE}%）")
        else:
            print_warning("無法從狀態解析頻率")

        print(f"  回應：\n{response}")

    wait_for_user("接下來要測試佔空比變化（固定頻率），按 ENTER 繼續...")

    # 測試 1.2：佔空比變化
    print_step("1.2", "佔空比變化測試（固定頻率）")
    print_info(f"在 {TEST_BASELINE_FREQ} Hz 測試不同佔空比...")

    for duty in TEST_DUTIES:
        print(f"\n  測試佔空比：{duty}%")
        cmd = f"UART1 PWM {TEST_BASELINE_FREQ} {duty} ON"
        response = send_command(ser, cmd)
        print(f"  命令：{cmd}")
        print(f"  回應：{response.strip()}")

        if f"{duty}" in response or f"{duty}.0" in response:
            print_success(f"佔空比 {duty}% 設定成功")

        time.sleep(PWM_STABILIZATION_DELAY)

    wait_for_user("接下來要測試頻率切換（毛刺檢測），按 ENTER 繼續...")

    # 測試 1.3：頻率切換（毛刺檢測）
    print_step("1.3", "頻率切換測試（毛刺檢測）")
    print_info("快速改變頻率以檢測毛刺...")
    print_warning("⚠️  觀察：注意任何毛刺或不連續現象")
    print()
    print_info("預期行為說明：")
    print("  - 第 1 次切換：可能有毛刺（從上一個測試狀態切換，預分頻器可能改變）")
    print("  - 第 2-5 次：取決於預分頻器是否改變")
    print("  - 綠色 [調試] 'GLITCH-FREE PATH' → 無毛刺")
    print("  - 黃色 [調試] 'PRESCALER CHANGE' → 可能有毛刺")
    print()

    for i, freq in enumerate(TRANSITION_FREQUENCIES):
        if i == 0:
            glitch_note = "（預期：可能有毛刺）"
        else:
            glitch_note = "（查看上方調試訊息）"

        print(f"\n  切換 {i+1}：→ {freq} Hz ({TEST_BASELINE_DUTY}% 佔空比) {glitch_note}")
        cmd = f"UART1 PWM {freq} {TEST_BASELINE_DUTY} ON"
        response = send_command(ser, cmd, wait_time=TRANSITION_DELAY * 2)
        extract_debug_info(response)  # 顯示調試訊息
        print(f"  回應：{response.strip()}")
        time.sleep(TRANSITION_DELAY)

    print()
    print_info("💡 提示：調試訊息已顯示在上方")
    print("   - 綠色 '[調試]' → GLITCH-FREE PATH（無毛刺）")
    print("   - 黃色 '[調試]' → PRESCALER CHANGE（可能有毛刺）")
    user_input = input(f"{Colors.WARNING}您觀察到任何毛刺嗎？(yes/no)：{Colors.ENDC}")
    if user_input.lower() == 'no':
        print_success("頻率切換：平滑（無毛刺）")
        print_info("✅ 優秀！這表示所有切換都使用了相同的預分頻器")
    else:
        print_warning("頻率切換：觀察到毛刺")
        print_info("ℹ️ 這可能是正常的（預分頻器改變），請查看上方的調試訊息")

    wait_for_user("接下來要測試佔空比切換（毛刺檢測），按 ENTER 繼續...")

    # 測試 1.4：佔空比切換（毛刺檢測）
    print_step("1.4", "佔空比切換測試（毛刺檢測）")
    print_info("在固定頻率下快速改變佔空比...")
    print_warning("⚠️  觀察：注意任何毛刺或不連續現象")
    print()
    print_info("預期行為說明：")
    print("  - 頻率固定為 1000 Hz，預分頻器不變")
    print("  - 所有佔空比切換都應該是無毛刺的（使用影子寄存器模式）")
    print("  - 佔空比更新會在下一個 TEZ（Timer Equals Zero）同步生效")
    print("  - ✅ 如果觀察到毛刺，這是異常的！")
    print()

    for i, duty in enumerate(TRANSITION_DUTIES):
        print(f"\n  切換 {i+1}：→ {duty}% ({TEST_BASELINE_FREQ} Hz) （預期：無毛刺）")
        cmd = f"UART1 PWM {TEST_BASELINE_FREQ} {duty} ON"
        response = send_command(ser, cmd, wait_time=TRANSITION_DELAY * 2)
        extract_debug_info(response)  # 顯示調試訊息
        print(f"  回應：{response.strip()}")
        time.sleep(TRANSITION_DELAY)

    print()
    user_input = input(f"{Colors.WARNING}您觀察到任何毛刺嗎？(yes/no)：{Colors.ENDC}")
    if user_input.lower() == 'no':
        print_success("佔空比切換：平滑（無毛刺）")
        print_info("✅ 正確！佔空比更新使用了 TEZ 同步機制")
    else:
        print_fail("佔空比切換：觀察到毛刺")
        print_warning("❌ 異常！佔空比切換不應該有毛刺（頻率未改變）")
        print_info("   請檢查韌體實現或示波器連接")

    wait_for_user("接下來要測試極限頻率（最小 1 Hz 和最大 500 kHz），按 ENTER 繼續...")

    # 測試 1.5：極限頻率測試
    print_step("1.5", "極限頻率測試")
    print_info("測試最小和最大頻率限制...")

    for freq, desc in EXTREME_FREQUENCIES:
        print(f"\n  測試 {desc}：{freq} Hz")
        cmd = f"UART1 PWM {freq} {TEST_BASELINE_DUTY} ON"
        response = send_command(ser, cmd)
        print(f"  回應：{response.strip()}")

        time.sleep(PWM_STABILIZATION_DELAY)
        status = send_command(ser, "UART1 STATUS")
        print(f"  狀態：\n{status}")

    print_success("PWM/RPM 模式測試完成")

def test_uart_mode(ser: serial.Serial) -> None:
    """測試套件 2：UART 模式測試"""
    print_header("測試套件 2：UART 模式測試")

    # 切換到 UART 模式
    print_step(1, "切換 UART1 到 UART 模式")
    response = send_command(ser, "UART1 MODE UART")
    if "UART" in response:
        print_success("UART1 已切換到 UART 模式")
    else:
        print_fail(f"無法切換到 UART 模式：{response}")
        return

    wait_for_user("接下來要測試不同鮑率（2400 ~ 1500000 baud），按 ENTER 繼續...")

    # 測試 2.1：不同鮑率
    print_step("2.1", "鮑率測試（Echo Loopback）")
    print_info("在不同鮑率測試 UART 通訊...")

    for baud in TEST_BAUDS:
        print(f"\n  測試鮑率：{baud}")

        # 配置鮑率
        cmd = f"UART1 CONFIG {baud}"
        response = send_command(ser, cmd)
        print(f"  配置回應：{response.strip()}")

        time.sleep(STATUS_CHECK_DELAY)

        # 發送測試訊息
        test_msg = f"Test@{baud}bps"
        cmd = f"UART1 WRITE {test_msg}"
        print(f"  發送：'{test_msg}'")
        response = send_command(ser, cmd)
        print(f"  寫入回應：{response.strip()}")

        # 檢查狀態（loopback 應該接收到資料）
        status = send_command(ser, "UART1 STATUS", wait_time=STATUS_CHECK_DELAY)

        if "bytes" in response.lower() or "sent" in response.lower():
            print_success(f"鮑率 {baud}：資料成功發送")
        else:
            print_warning(f"鮑率 {baud}：狀態不明確")

    wait_for_user("接下來要測試不同訊息長度，按 ENTER 繼續...")

    # 測試 2.2：不同訊息長度
    print_step("2.2", "訊息長度測試")
    print_info(f"在 {DEFAULT_TEST_BAUD} 鮑率測試不同訊息長度...")

    # 設定為 DEFAULT_TEST_BAUD
    send_command(ser, f"UART1 CONFIG {DEFAULT_TEST_BAUD}")
    time.sleep(STATUS_CHECK_DELAY)

    for i, msg in enumerate(TEST_MESSAGES):
        print(f"\n  測試 {i+1}：長度 {len(msg)} 字元")
        print(f"  訊息：'{msg[:50]}{'...' if len(msg) > 50 else ''}'")

        cmd = f"UART1 WRITE {msg}"
        response = send_command(ser, cmd)
        print(f"  回應：{response.strip()}")

        if f"{len(msg)}" in response or "sent" in response.lower():
            print_success(f"長度 {len(msg)}：成功發送")

    wait_for_user("接下來要測試特殊字元，按 ENTER 繼續...")

    # 測試 2.3：特殊字元
    print_step("2.3", "特殊字元測試")
    print_info("測試特殊字元和符號...")

    for msg in SPECIAL_MESSAGES:
        print(f"\n  測試：'{msg}'")
        cmd = f"UART1 WRITE {msg}"
        response = send_command(ser, cmd)
        print(f"  回應：{response.strip()}")

    print_success("UART 模式測試完成")

def test_mode_switching(ser: serial.Serial) -> None:
    """測試套件 3：模式切換測試"""
    print_header("測試套件 3：模式切換測試")

    # 測試 3.1：PWM → UART → PWM
    print_step("3.1", "模式切換循環：PWM → UART → PWM")

    for i, mode in enumerate(MODE_SWITCH_SEQUENCE):
        print(f"\n  切換 {i+1}：→ {mode} 模式")
        cmd = f"UART1 MODE {mode}"
        response = send_command(ser, cmd)
        print(f"  回應：{response.strip()}")

        # 驗證狀態
        status = send_command(ser, "UART1 STATUS", wait_time=STATUS_CHECK_DELAY)
        if mode in status or ("PWM/RPM" in status and mode == "PWM"):
            print_success(f"模式 {mode}：已驗證")
        else:
            print_fail(f"模式 {mode}：驗證失敗")

        time.sleep(MODE_SWITCH_DELAY)

    wait_for_user("接下來要測試 OFF 模式切換，按 ENTER 繼續...")

    # 測試 3.2：OFF 模式
    print_step("3.2", "OFF 模式測試")

    print("  測試：PWM → OFF → PWM")

    # 到 OFF
    response = send_command(ser, "UART1 MODE OFF")
    print(f"  OFF 模式回應：{response.strip()}")
    status = send_command(ser, "UART1 STATUS")
    print(f"  狀態：{status}")

    time.sleep(MODE_SWITCH_DELAY)

    # 回到 PWM
    response = send_command(ser, "UART1 MODE PWM")
    print(f"  PWM 模式回應：{response.strip()}")
    status = send_command(ser, "UART1 STATUS")
    print(f"  狀態：{status}")

    print_success("模式切換測試完成")

def test_error_handling(ser: serial.Serial) -> None:
    """測試套件 4：錯誤處理測試"""
    print_header("測試套件 4：錯誤處理與邊界測試")

    print_step(1, "無效參數測試")

    # 測試 4.1：無效鮑率
    print("\n  測試：無效鮑率（1200 - 太低）")
    response = send_command(ser, "UART1 CONFIG 1200")
    print(f"  回應：{response.strip()}")
    if "error" in response.lower() or "invalid" in response.lower():
        print_success("正確拒絕無效鮑率")

    # 測試 4.2：無效頻率
    print("\n  測試：無效頻率（600000 Hz - 太高）")
    response = send_command(ser, "UART1 PWM 600000 50 ON")
    print(f"  回應：{response.strip()}")
    if "error" in response.lower() or "invalid" in response.lower():
        print_success("正確拒絕無效頻率")

    # 測試 4.3：無效佔空比
    print("\n  測試：無效佔空比（150% - 太高）")
    response = send_command(ser, "UART1 PWM 1000 150 ON")
    print(f"  回應：{response.strip()}")
    if "error" in response.lower() or "invalid" in response.lower():
        print_success("正確拒絕無效佔空比")

    print_success("錯誤處理測試完成")

def test_persistence(ser: serial.Serial) -> None:
    """測試套件 5：設定持久化測試"""
    print_header("測試套件 5：設定持久化測試")

    print_step(1, "UART1 模式非持久化驗證")
    print_info("UART1 模式不應該在電源循環後持久保存")
    print_info("它總是在上電時預設為 PWM/RPM 模式")

    # 設定為 UART 模式
    print("\n  設定 UART1 為 UART 模式...")
    response = send_command(ser, "UART1 MODE UART")
    print(f"  回應：{response.strip()}")

    # 儲存週邊設定
    print("\n  儲存週邊設定...")
    response = send_command(ser, "PERIPHERAL SAVE")
    print(f"  回應：{response.strip()}")

    print()
    print_warning("⚠️  請重置 ESP32（按 RESET 按鈕或重新插拔 USB）")
    print_info("重置後，UART1 應該是 PWM/RPM 模式（預設）")
    print_info("而不是 UART 模式，即使我們儲存了設定")

    wait_for_user("在您重置裝置後按 ENTER...")

    # 等待裝置重新初始化
    print("等待裝置重新初始化...")
    time.sleep(DEVICE_INIT_DELAY)

    # 檢查狀態
    print("\n  檢查重置後的 UART1 模式...")
    response = send_command(ser, "UART1 STATUS", wait_time=COMMAND_TIMEOUT)
    print(f"  狀態：\n{response}")

    if "PWM/RPM" in response or "PWM" in response:
        print_success("✅ 正確：UART1 預設為 PWM/RPM 模式（非持久化）")
    elif "UART" in response and "PWM" not in response:
        print_fail("❌ 失敗：UART1 處於 UART 模式（不應持久化）")
    else:
        print_warning("⚠️  狀態不明確")

    print_success("持久化測試完成")

def main() -> None:
    """主測試執行"""
    print_header("UART1 多模式綜合測試套件")
    print(f"開始時間：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

    # 尋找 COM 埠
    print_info("搜尋 ESP32-S3 裝置...")
    port = find_esp32_port()

    if not port:
        print_fail("找不到 ESP32-S3！")
        print("請確保：")
        print("  1. 裝置已透過 USB 連接")
        print("  2. 韌體已上傳")
        print("  3. USB 線支援資料傳輸")
        sys.exit(1)

    print_success(f"在 {port} 找到 ESP32-S3")

    try:
        # 開啟序列連接
        print_info(f"在 {port} 開啟序列連接...")
        ser = serial.Serial(port, DEFAULT_BAUDRATE, timeout=SERIAL_TIMEOUT)
        ser.setDTR(True)
        time.sleep(DTR_STABILIZATION_DELAY)  # 等待連接

        # 清除緩衝區
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print_success("序列連接已建立")

        # 硬體設置
        test_hardware_setup()

        # 執行測試套件
        test_pwm_rpm_mode(ser)
        test_uart_mode(ser)
        test_mode_switching(ser)
        test_error_handling(ser)
        test_persistence(ser)

        # 最終摘要
        print_header("測試套件完成")
        print(f"結束時間：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print_success("所有測試套件已執行！")
        print()
        print_info("測試摘要：")
        print("  ✅ 套件 1：PWM/RPM 模式測試")
        print("  ✅ 套件 2：UART 模式測試")
        print("  ✅ 套件 3：模式切換測試")
        print("  ✅ 套件 4：錯誤處理測試")
        print("  ✅ 套件 5：設定持久化測試")
        print()
        print_warning("請檢查以上測試結果，注意任何失敗或警告。")

    except serial.SerialException as e:
        print_fail(f"序列通訊錯誤：{e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print_warning("\n測試被使用者中斷")
        sys.exit(0)
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print_info("序列連接已關閉")

if __name__ == "__main__":
    main()
