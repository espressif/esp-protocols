#!/usr/bin/env python3
"""
Serial monitor script for Autobahn test suite.
Monitors ESP32 serial output and detects test completion.
"""

import serial
import sys
import time
import re
import argparse


def main():
    parser = argparse.ArgumentParser(description='Monitor ESP32 serial output for Autobahn test completion')
    parser.add_argument('--port', '-p', 
                       default='/dev/ttyUSB0',
                       help='Serial port (default: /dev/ttyUSB0)')
    parser.add_argument('--baud', '-b',
                       type=int,
                       default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--timeout', '-t',
                       type=int,
                       default=2400,
                       help='Timeout in seconds (default: 2400 = 40 minutes)')
    parser.add_argument('--completion-pattern', '-c',
                       default=r'All tests completed\.',
                       help='Regex pattern to detect completion (default: "All tests completed.")')
    parser.add_argument('--uri', '-u',
                       default=None,
                       help='Server URI to send via serial (stdin). If provided, will send this URI after opening port.')
    
    args = parser.parse_args()
    
    port = args.port
    timeout_seconds = args.timeout
    completion_pattern = re.compile(args.completion_pattern)
    
    print(f"Opening serial port: {port} at {args.baud} baud")
    try:
        ser = serial.Serial(port, args.baud, timeout=1)
        print("Serial port opened successfully")
        
        # If URI is provided, send it via serial (stdin)
        if args.uri:
            print(f"Sending server URI: {args.uri}")
            # Wait a bit for ESP32 to be ready
            time.sleep(2)
            # Send URI followed by newline
            ser.write(f"{args.uri}\n".encode('utf-8'))
            ser.flush()
            print("URI sent successfully")
        
        buffer = ""
        start_time = time.time()
        
        while True:
            elapsed = time.time() - start_time
            if elapsed > timeout_seconds:
                print(f"\n⚠ Timeout after {timeout_seconds}s - tests may still be running")
                sys.exit(1)
            
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data
                sys.stdout.write(data)
                sys.stdout.flush()
                
                # Check for completion message
                if completion_pattern.search(buffer):
                    print("\n✓ Test suite completed successfully!")
                    time.sleep(5)  # Wait a bit more for any final output
                    sys.exit(0)
            
            time.sleep(0.1)
        
        ser.close()
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)


if __name__ == '__main__':
    main()

