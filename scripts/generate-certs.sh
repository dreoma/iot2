#!/usr/bin/env sh
set -eu

CERT_DIR="$(dirname "$0")/../certs"
mkdir -p "$CERT_DIR"
umask 077

case "${1-}" in
    "")
        if [ -e "$CERT_DIR/ca.crt" ] || [ -e "$CERT_DIR/server.crt" ]; then
            echo "Certificates already exist in $CERT_DIR"
            exit 0
        fi
        openssl genrsa -out "$CERT_DIR/ca.key" 4096
        openssl req -x509 -new -key "$CERT_DIR/ca.key" -sha256 -days 3650 \
            -subj "/CN=iot-local-ca" -out "$CERT_DIR/ca.crt"
        ;;
    --renew-server)
        if [ ! -f "$CERT_DIR/ca.key" ] || [ ! -f "$CERT_DIR/ca.crt" ]; then
            echo "The existing CA certificate and key are required" >&2
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 [--renew-server]" >&2
        exit 1
        ;;
esac

work_dir=$(mktemp -d "$CERT_DIR/server.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

openssl genrsa -out "$work_dir/server.key" 2048
openssl req -new -key "$work_dir/server.key" -subj "/CN=core-tls" -out "$work_dir/server.csr"
printf 'subjectAltName=DNS:core-tls,DNS:localhost,IP:127.0.0.1\n' > "$work_dir/server.ext"
openssl x509 -req -in "$work_dir/server.csr" -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" -CAserial "$work_dir/ca.srl" -CAcreateserial -out "$work_dir/server.crt" \
    -days 825 -sha256 -extfile "$work_dir/server.ext"
chmod 644 "$work_dir/server.crt"
mv "$work_dir/server.key" "$CERT_DIR/server.key"
mv "$work_dir/server.crt" "$CERT_DIR/server.crt"
echo "Server certificate created in $CERT_DIR for core-tls and localhost"
