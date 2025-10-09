#!/bin/bash
# Generate CA, Server, and Client certificates automatically

# Create directories if they don't exist
mkdir -p main/certs/server

echo "Generating CA certificate..."
openssl genrsa -out main/certs/ca_key.pem 2048
openssl req -new -x509 -days 3650 -key main/certs/ca_key.pem -out main/certs/ca_cert.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=TestCA"

echo "Generating Server certificate..."
openssl genrsa -out main/certs/server/server_key.pem 2048
openssl req -new -key main/certs/server/server_key.pem -out server_csr.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=localhost"
openssl x509 -req -days 3650 -in server_csr.pem -CA main/certs/ca_cert.pem -CAkey main/certs/ca_key.pem -CAcreateserial -out main/certs/server/server_cert.pem

echo "Generating Client certificate..."
openssl genrsa -out main/certs/client_key.pem 2048
openssl req -new -key main/certs/client_key.pem -out client_csr.pem -subj "/C=US/ST=State/L=City/O=Organization/CN=TestClient"
openssl x509 -req -days 3650 -in client_csr.pem -CA main/certs/ca_cert.pem -CAkey main/certs/ca_key.pem -CAcreateserial -out main/certs/client_cert.pem

# Clean up CSR files
rm server_csr.pem client_csr.pem

echo "Certificates generated successfully!"
echo "Generated files:"
echo "  - main/certs/ca_cert.pem (CA certificate)"
echo "  - main/certs/ca_key.pem (CA private key)"
echo "  - main/certs/client_cert.pem (Client certificate)"
echo "  - main/certs/client_key.pem (Client private key)"
echo "  - main/certs/server/server_cert.pem (Server certificate)"
echo "  - main/certs/server/server_key.pem (Server private key)"
