#!/bin/bash
# Empaqueta el binario como .vcvplugin (tar + zstd, el formato que Rack descomprime solo al arrancar)
# uso: empaquetar.sh <binario> <sistema-cpu>   p. ej. build/plugin.dylib mac-arm64
set -e
BIN="$1"; ARCH="$2"
SLUG=$(python3 -c "import json;print(json.load(open('plugin.json'))['slug'])")
VER=$(python3 -c "import json;print(json.load(open('plugin.json'))['version'])")
T=$(mktemp -d)
mkdir -p "$T/$SLUG" dist
cp "$BIN" "$T/$SLUG/"
cp plugin.json LICENSE* "$T/$SLUG/"
cp -R res "$T/$SLUG/"
OUT="$PWD/dist/$SLUG-$VER-$ARCH.vcvplugin"
(cd "$T" && tar -cf - "$SLUG" | zstd -19 -q -f -o "$OUT")
rm -rf "$T"
echo "empaquetado: $OUT"
