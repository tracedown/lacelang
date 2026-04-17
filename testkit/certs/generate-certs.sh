#!/bin/sh
# generate-certs.sh — produce the TLS cert scenarios the testkit's HTTPS
# mock server loads per vector.
#
# The harness auto-generates certs to a temp directory on each run
# (cleaned up at exit). Users can also pre-generate via
# `lace-conformance --generate-certs` for CI caching.
#
# Requires openssl on PATH — exits non-zero if missing.
#
# Scenarios produced (all in $CERTS_DIR):
#   ca.pem  / ca.key            — test CA (not trusted by any OS)
#   valid.pem  / valid.key      — server cert, CN=127.0.0.1, signed by CA
#   expired.pem  / expired.key  — cert whose validity window ended in 2000
#   wrong_host.pem / wrong_host.key — CN=wronghost.test, signed by CA
#   self_signed.pem / self_signed.key — self-signed, no CA chain

set -eu

CERTS_DIR="${1:-$(dirname "$0")}"

if ! command -v openssl >/dev/null 2>&1; then
    echo "lacelang-testkit: ERROR — openssl not found on PATH." >&2
    echo "                  TLS test certs cannot be generated without openssl." >&2
    echo "                  Install openssl: apt install openssl / brew install openssl" >&2
    exit 1
fi

mkdir -p "$CERTS_DIR"
cd "$CERTS_DIR"

if [ -f valid.pem ] && [ -f expired.pem ] && [ -f wrong_host.pem ] && [ -f self_signed.pem ]; then
    echo "lacelang-testkit: certs already present in $CERTS_DIR — not regenerating." >&2
    exit 0
fi

# -- Root CA --------------------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem \
    -days 3650 -subj "/CN=lacelang-testkit-ca" >/dev/null 2>&1

# Helper: issue a CA-signed cert with SAN.
issue_ca_cert() {
    name="$1"; cn="$2"; days="$3"; san="$4"
    openssl req -newkey rsa:2048 -nodes -keyout "$name.key" -out "$name.csr" \
        -subj "/CN=$cn" >/dev/null 2>&1
    ext="$(mktemp)"
    printf 'subjectAltName=%s\nextendedKeyUsage=serverAuth\n' "$san" >"$ext"
    openssl x509 -req -in "$name.csr" -CA ca.pem -CAkey ca.key -CAcreateserial \
        -out "$name.pem" -days "$days" -extfile "$ext" >/dev/null 2>&1
    rm -f "$name.csr" "$ext"
}

# -- valid: CA-signed, CN=127.0.0.1 --------------------------------------
issue_ca_cert valid      127.0.0.1       3650 "IP:127.0.0.1,DNS:localhost"

# -- wrong_host: CA-signed but CN=wronghost.test -------------------------
issue_ca_cert wrong_host wronghost.test  3650 "DNS:wronghost.test"

# -- self_signed: separate chain, not issued by our CA -------------------
openssl req -x509 -newkey rsa:2048 -nodes -keyout self_signed.key -out self_signed.pem \
    -days 3650 -subj "/CN=127.0.0.1" \
    -addext "subjectAltName=IP:127.0.0.1,DNS:localhost" >/dev/null 2>&1

# -- expired: CA-signed with an elapsed validity window ------------------
# Uses `openssl ca` so we can hand it explicit startdate/enddate. Falls back
# to a regular `-days 1` signing (which will show up as not-yet-expired on
# most clocks) if the CA machinery isn't available on this system.
openssl req -newkey rsa:2048 -nodes -keyout expired.key -out expired.csr \
    -subj "/CN=127.0.0.1" >/dev/null 2>&1
expired_ext="$(mktemp)"
printf 'subjectAltName=IP:127.0.0.1,DNS:localhost\nextendedKeyUsage=serverAuth\n' >"$expired_ext"
expired_conf="$(mktemp)"
cat >"$expired_conf" <<'CONF'
[ca]
default_ca = testkit_expired
[testkit_expired]
new_certs_dir = .
database = ./expired.db
serial = ./expired.srl
certificate = ./ca.pem
private_key = ./ca.key
default_days = 1
default_md = sha256
policy = any_policy
email_in_dn = no
copy_extensions = copy
x509_extensions = expired_exts
[any_policy]
commonName = supplied
[expired_exts]
subjectAltName = IP:127.0.0.1,DNS:localhost
extendedKeyUsage = serverAuth
CONF
: > expired.db
echo 01 > expired.srl
if ! openssl ca -config "$expired_conf" -batch \
        -in expired.csr -out expired.pem \
        -startdate 19000101000000Z -enddate 20000101000000Z \
        >/dev/null 2>&1; then
    openssl x509 -req -in expired.csr -CA ca.pem -CAkey ca.key -CAcreateserial \
        -out expired.pem -days 1 -extfile "$expired_ext" >/dev/null 2>&1
    echo "lacelang-testkit: WARNING — 'openssl ca' rejected pre-epoch dates; emitted a" >&2
    echo "                  short-lived 'expired' cert instead. TLS-expired vectors may" >&2
    echo "                  pass handshake. Upgrade openssl to >= 1.1.1 for strict expiry." >&2
fi
rm -f expired.csr expired.db* expired.srl expired.srl.old "$expired_ext" "$expired_conf"
# `openssl ca` also drops per-serial PEM copies (01.pem, 02.pem, ...) when it
# commits the issue — those aren't part of our scenario set.
rm -f [0-9][0-9].pem

chmod 0644 *.pem 2>/dev/null || true
chmod 0600 *.key 2>/dev/null || true

echo "lacelang-testkit: generated TLS test certs in $CERTS_DIR" >&2
