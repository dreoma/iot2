#!/usr/bin/env sh
set -eu

CERT_DIR="$(dirname "$0")/../certs"
mkdir -p "$CERT_DIR"

if [ -e "$CERT_DIR/ca.crt" ] || [ -e "$CERT_DIR/server.crt" ]; then
    echo "Certificates already exist in $CERT_DIR"
    exit 0
fi

# Local CA and a server certificate valid for the Docker service core-tls.
openssl genrsa -out "$CERT_DIR/ca.key" 4096
openssl req -x509 -new -key "$CERT_DIR/ca.key" -sha256 -days 3650 \
    -subj "/CN=iot-local-ca" -out "$CERT_DIR/ca.crt"
openssl genrsa -out "$CERT_DIR/server.key" 2048
openssl req -new -key "$CERT_DIR/server.key" -subj "/CN=core-tls" \
    -out "$CERT_DIR/server.csr"
printf 'subjectAltName=DNS:core-tls\n' > "$CERT_DIR/server.ext"
openssl x509 -req -in "$CERT_DIR/server.csr" -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" -CAserial "$CERT_DIR/ca.srl" -CAcreateserial -out "$CERT_DIR/server.crt" \
    -days 825 -sha256 -extfile "$CERT_DIR/server.ext"
rm "$CERT_DIR/server.csr" "$CERT_DIR/server.ext" "$CERT_DIR/ca.srl"
echo "Certificates created in $CERT_DIR"
