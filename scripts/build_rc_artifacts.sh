#!/usr/bin/env bash
# 构建 RC 固件与 LittleFS，并输出可追溯的交付物清单。
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$project_root/.pio/build/esp32-c3-devkitc-02"
artifact_dir="$project_root/dist/rc1"

cd "$project_root"
pio run
pio run --target buildfs

mkdir -p "$artifact_dir"
cp "$build_dir/firmware.bin" "$artifact_dir/clawd-mochi-1.0.0-rc1-firmware.bin"
cp "$build_dir/littlefs.bin" "$artifact_dir/clawd-mochi-1.0.0-rc1-littlefs.bin"

(
    cd "$artifact_dir"
    shasum -a 256 \
        clawd-mochi-1.0.0-rc1-firmware.bin \
        clawd-mochi-1.0.0-rc1-littlefs.bin > SHA256SUMS
)

cat > "$artifact_dir/manifest.json" <<EOF
{
  "product": "Clawd Mochi",
  "version": "1.0.0-rc1",
  "target": "esp32-c3-devkitc-02",
  "firmware": "clawd-mochi-1.0.0-rc1-firmware.bin",
  "littlefs": "clawd-mochi-1.0.0-rc1-littlefs.bin",
  "checksums": "SHA256SUMS"
}
EOF

(cd "$artifact_dir" && shasum -a 256 -c SHA256SUMS)
echo "RC artifacts verified: $artifact_dir"
