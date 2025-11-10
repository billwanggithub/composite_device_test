"""
Simple BLE client for the project's BLE GATT "serial" console.

Usage:
  python scripts/ble_client.py --scan                        # Scan for BLE devices
  python scripts/ble_client.py --name BillCat_Fan_Control    # Connect by name
  python scripts/ble_client.py --address AA:BB:CC:DD:EE:FF   # Connect by address

The client will:
 - connect to the device by name or address
 - start notifications on the TX characteristic and print incoming messages
 - read lines from stdin and write them to the RX characteristic

Dependencies:
 - bleak (pip install bleak)

Works on Windows/macOS/Linux (requires a working BLE adapter and python3.8+).

Response Routing (v2.2):
------------------------
Commands sent via BLE will have different response behavior:

1. SCPI commands (*IDN?, *RST, etc.):
   - Responses are sent back via BLE TX characteristic (notify)
   - You will see the response in this client

2. General commands (HELP, INFO, STATUS, etc.):
   - Responses are sent to CDC interface only (for centralized monitoring)
   - You will NOT see responses in this BLE client
   - Check the device's CDC serial output to see responses

This is by design for better debugging/monitoring!
"""

import argparse
import asyncio
import sys
from typing import Optional
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# ==================== 配置常數 ====================
# BLE GATT Service and Characteristic UUIDs
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Write (RX on device)
CHAR_UUID_TX = "beb5483e-36e1-4688-b7f5-ea07361b26a9"  # Notify (TX from device)

# Scan and connection timeouts
DEFAULT_SCAN_TIMEOUT = 8.0  # Seconds to scan for BLE devices
CONNECTION_RETRY_DELAY = 1.0  # Delay before retrying connection (if needed)

async def scan_devices(timeout: float = DEFAULT_SCAN_TIMEOUT) -> None:
    """Scan for all BLE devices and display them"""
    print(f"Scanning for BLE devices (timeout: {timeout}s)...")
    print("=" * 60)
    devices = await BleakScanner.discover(timeout=timeout)

    if not devices:
        print("No BLE devices found.")
        return

    print(f"Found {len(devices)} device(s):")
    print("-" * 60)
    for i, d in enumerate(devices, 1):
        name = d.name if d.name else "<Unknown>"
        print(f"{i}. Name: {name}")
        print(f"   Address: {d.address}")
        print(f"   RSSI: {d.rssi} dBm")
        print()
    print("=" * 60)

async def scan_for_name(name: str, timeout: float = DEFAULT_SCAN_TIMEOUT) -> Optional[str]:
    """Scan for BLE device by name substring match"""
    devices = await BleakScanner.discover(timeout=timeout)
    for d in devices:
        if d.name and name in d.name:
            return d.address
    return None

async def run(address: Optional[str] = None, name: Optional[str] = None) -> None:
    """Connect to BLE device and handle interactive communication"""
    if not address and name:
        print(f"Scanning for BLE device with name containing: {name}...")
        address = await scan_for_name(name)
        if not address:
            print("Device not found via scan")
            return
        print(f"Found device: {address}")

    if not address:
        print("No device address provided and name scan failed; exiting.")
        return

    async with BleakClient(address) as client:
        if not client.is_connected:
            print("Failed to connect")
            return
        print(f"Connected to {address}")

        # notification handler
        def handle_notification(sender: int, data: bytearray) -> None:
            """Handle incoming BLE notifications from TX characteristic"""
            try:
                text = data.decode("utf-8", errors="replace")
            except UnicodeDecodeError:
                # Fallback to hex representation if decode fails
                text = repr(data)
            # Print and flush so user sees messages immediately
            print(text, end="", flush=True)

        await client.start_notify(CHAR_UUID_TX, handle_notification)
        print("Subscribed to TX notifications. Type lines and press Enter to send to RX characteristic.")
        print()
        print("⚠️  Response Routing Info:")
        print("   - SCPI commands (*IDN?, etc.) → Response via BLE")
        print("   - General commands (HELP, INFO, etc.) → Response via CDC only")
        print()

        loop = asyncio.get_event_loop()
        try:
            while True:
                # Read a line from stdin in a thread pool to avoid blocking the event loop
                line = await loop.run_in_executor(None, sys.stdin.readline)
                if not line:
                    # EOF
                    break
                # ensure newline
                if not line.endswith("\n"):
                    line = line + "\n"
                try:
                    await client.write_gatt_char(CHAR_UUID_RX, line.encode("utf-8"), response=False)
                except BleakError as e:
                    print(f"BLE write error: {e}")
                    break
                except UnicodeEncodeError as e:
                    print(f"Unicode encode error: {e}")
                    continue  # Skip this line but continue running
        except KeyboardInterrupt:
            print("\nInterrupted by user")
        finally:
            await client.stop_notify(CHAR_UUID_TX)
            print("Disconnected")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="BLE serial test client")
    parser.add_argument("--scan", action="store_true", help="Scan for BLE devices and exit")
    parser.add_argument("--name", help="BLE device name to scan for (substring match)")
    parser.add_argument("--address", help="BLE device address (skip scanning)")
    args = parser.parse_args()

    try:
        if args.scan:
            # Just scan and display devices
            asyncio.run(scan_devices())
        else:
            # Connect to device
            asyncio.run(run(address=args.address, name=args.name))
    except BleakError as e:
        print(f"BLE error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)
