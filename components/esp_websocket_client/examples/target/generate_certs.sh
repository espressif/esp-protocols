#!/bin/bash
# Generate CA, Server, and Client certificates automatically
#
# Usage: ./generate_certs.sh [SERVER_CN]
#   SERVER_CN: The Common Name (hostname or IP) for the server certificate.
#              This should match the hostname/IP that ESP32 clients will use to connect.
#              If not provided, the script will attempt to auto-detect the local IP address.
#              Falls back to "localhost" if auto-detection fails.
#
# IMPORTANT: The server certificate's Common Name (CN) must match the hostname or IP address
# that ESP32 clients use to connect. If there's a mismatch, certificate verification will fail
# during the TLS handshake. For production use, always specify the correct hostname/IP.

# Get server hostname/IP from command line argument or auto-detect
if [ -n "$1" ]; then
    SERVER_CN="$1"
    echo "Using provided SERVER_CN: $SERVER_CN"
else
    # Attempt to auto-detect local IP address
    # Try multiple methods for better compatibility across different systems
    if command -v hostname >/dev/null 2>&1; then
        # Try to get IP from hostname command (works on most Unix systems)
        SERVER_CN=$(hostname -I 2>/dev/null | awk '{print $1}')
    fi

    # If the above failed, try ifconfig (macOS and some Linux systems)
    if [ -z "$SERVER_CN" ] && command -v ifconfig >/dev/null 2>&1; then
        SERVER_CN=$(ifconfig | grep "inet " | grep -v 127.0.0.1 | awk '{print $2}' | head -n1)
    fi

    # If still empty, try ip command (modern Linux systems)
    if [ -z "$SERVER_CN" ] && command -v ip >/dev/null 2>&1; then
        SERVER_CN=$(ip -4 addr show | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | grep -v 127.0.0.1 | head -n1)
    fi

    # Fall back to localhost if auto-detection failed
    if [ -z "$SERVER_CN" ]; then
        SERVER_CN="localhost"
        echo "Warning: Could not auto-detect IP address. Using 'localhost' as SERVER_CN."
        echo "         If your server runs on a different machine or IP, re-run with: ./generate_certs.sh <hostname_or_ip>"
    else
        echo "Auto-detected SERVER_CN: $SERVER_CN"
    fi
fi

echo "Note: ESP32 clients must connect using: $SERVER_CN"
echo ""

# Create directories if they don't exist
mkdir -p main/certs/server

echo "Generating CA certificate..."
openssl genrsa -out main/certs/ca_key.pem 2048
openssl req -new -x509 -days 3650 -key main/certs/ca_key.pem -out main/certs/ca_cert.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=TestCA"

echo "Generating Server certificate with CN=$SERVER_CN..."
openssl genrsa -out main/certs/server/server_key.pem 2048
openssl req -new -key main/certs/server/server_key.pem -out server_csr.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=$SERVER_CN"
openssl x509 -req -days 3650 -in server_csr.pem -CA main/certs/ca_cert.pem -CAkey main/certs/ca_key.pem -CAcreateserial -out main/certs/server/server_cert.pem

echo "Generating Client certificate..."
openssl genrsa -out main/certs/client_key.pem 2048
openssl req -new -key main/certs/client_key.pem -out client_csr.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=TestClient"
openssl x509 -req -days 3650 -in client_csr.pem -CA main/certs/ca_cert.pem -CAkey main/certs/ca_key.pem -CAcreateserial -out main/certs/client_cert.pem

# Clean up CSR files
rm server_csr.pem client_csr.pem

echo "Certificates generated successfully!"
echo ""
echo "Generated files:"
echo "  - main/certs/ca_cert.pem (CA certificate)"
echo "  - main/certs/ca_key.pem (CA private key)"
echo "  - main/certs/client_cert.pem (Client certificate)"
echo "  - main/certs/client_key.pem (Client private key)"
echo "  - main/certs/server/server_cert.pem (Server certificate with CN=$SERVER_CN)"
echo "  - main/certs/server/server_key.pem (Server private key)"
echo ""
echo "IMPORTANT: Configure ESP32 clients to connect to: $SERVER_CN"
echo "           The server certificate is valid for this hostname/IP only."
echo ""
echo "Note: If the CN doesn't match your connection hostname/IP, you have two options:"
echo "      1. Regenerate certificates with correct CN: ./generate_certs.sh <correct_hostname_or_ip>"
echo "      2. Skip CN verification (TESTING ONLY): Enable CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK=y"
echo "         WARNING: Option 2 reduces security and should NOT be used in production!"
