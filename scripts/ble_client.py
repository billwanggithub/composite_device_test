"""
Simple BLE client for the project's BLE GATT "serial" console.

Usage:
  python scripts/ble_client.py --name ESP32_S3_Console
  python scripts/ble_client.py --address AA:BB:CC:DD:EE:FF

The client will:
 - connect to the device by name or address
 - start notifications on the TX characteristic and print incoming messages
 - read lines from stdin and write them to the RX characteristic

Dependencies:
 - bleak (pip install bleak)

Works on Windows/macOS/Linux (requires a working BLE adapter and python3.8+).
"""

import argparse
import asyncio
import sys
from bleak import BleakScanner, BleakClient

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # write
CHAR_UUID_TX = "beb5483e-36e1-4688-b7f5-ea07361b26a9"  # notify

async def scan_for_name(name, timeout=8.0):
    devices = await BleakScanner.discover(timeout=timeout)
    for d in devices:
        if d.name and name in d.name:
            return d.address
    return None

async def run(address=None, name=None):
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
        def handle_notification(sender, data: bytearray):
            try:
                text = data.decode("utf-8", errors="replace")
            except Exception:
                text = repr(data)
            # Print and flush so user sees messages immediately
            print(text, end="", flush=True)

        await client.start_notify(CHAR_UUID_TX, handle_notification)
        print("Subscribed to TX notifications. Type lines and press Enter to send to RX characteristic.")

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
                except Exception as e:
                    print(f"Write error: {e}")
                    break
        except KeyboardInterrupt:
            print("\nInterrupted by user")
        finally:
            await client.stop_notify(CHAR_UUID_TX)
            print("Disconnected")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="BLE serial test client")
    parser.add_argument("--name", help="BLE device name to scan for (substring match)")
    parser.add_argument("--address", help="BLE device address (skip scanning)")
    args = parser.parse_args()

    try:
        asyncio.run(run(address=args.address, name=args.name))
    except Exception as e:
        print(f"Error: {e}")
