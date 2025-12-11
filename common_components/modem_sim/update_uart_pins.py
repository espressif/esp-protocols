#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import csv
import pathlib
import sys


def update_uart_pins(platform: str, module: str, tx_pin: str, rx_pin: str, csv_path: str) -> None:
    path = pathlib.Path(csv_path)
    rows = []
    found = False

    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames
        for row in reader:
            if row.get("platform") == platform and row.get("module_name") == module:
                if tx_pin:
                    row["uart_tx_pin"] = tx_pin
                if rx_pin:
                    row["uart_rx_pin"] = rx_pin
                found = True
            rows.append(row)

    if not found:
        print(f"Warning: no row updated for platform={platform} module={module}")

    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    if len(sys.argv) != 6:
        print("Usage: update_uart_pins.py <platform> <module> <uart_tx_pin> <uart_rx_pin> <csv_path>")
        return 1

    platform, module, tx_pin, rx_pin, csv_path = sys.argv[1:6]
    update_uart_pins(platform, module, tx_pin, rx_pin, csv_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
