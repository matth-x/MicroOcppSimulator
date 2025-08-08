#!/bin/sh
set -e

# Runtime API endpoint configuration
TARGET_FILE="./public/bundle.html.gz"
TEMP_HTML="/tmp/bundle.html"

if [ -z "$API_ROOT" ]; then
  echo "[warn] API_ROOT environment variable not detected, frontend will use {{API_ROOT}} placeholder"
else
  echo "[info] Runtime replacement: {{API_ROOT}} → $API_ROOT"
  # Decompress to temporary file
  gzip -d -c "$TARGET_FILE" > "$TEMP_HTML"
  # Replace all placeholders
  sed -i "s|{{API_ROOT}}|$API_ROOT|g" "$TEMP_HTML"
  # Recompress back to original path
  gzip -9 -c "$TEMP_HTML" > "$TARGET_FILE"
fi

# Start the C++ simulator
exec "./build/mo_simulator"