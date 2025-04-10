#!/bin/bash
# This script generates self-signed certificates for testing purposes only.
# DO NOT use these certificates in production environments.
# These certificates are meant for development and testing of SSL/TLS functionality.

# 1. Generate CA private key
openssl genrsa -out ca.key 2048

# 2. Generate CA certificate (validity: 20 years, CN=Espressif)
openssl req -x509 -new -nodes -key ca.key -sha256 -days 7300 -out ca.crt -subj "/C=AU/ST=Some-State/O=Internet Widgits Pty Ltd/CN=Espressif"

# 3. Generate server private key
openssl genrsa -out server.key 2048

# 4. Generate server Certificate Signing Request (CSR)
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"

# 5. Generate server certificate signed by CA (validity: 20 years)
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out srv.crt -days 7300 -sha256

# 6. Clean up intermediate files
rm server.csr ca.srl
