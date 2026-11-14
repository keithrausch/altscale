#!/usr/bin/env bash

# sudo apt install openssl
# sudo dnf install openssl

# move to script directory
cd "$(dirname "$(realpath "$0")")"

mkdir -p 'certs'

openssl req -x509 -newkey rsa:3072 \
  -keyout certs/key.pem \
  -out certs/cert.pem \
  -days 365 \
  -nodes \
  -sha256 \
  -subj "/CN=192.168.1.100" \
  -addext "subjectAltName=IP:192.168.1.100,DNS:localhost,IP:127.0.0.1"