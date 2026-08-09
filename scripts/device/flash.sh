#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if ! command -v pio >/dev/null 2>&1; then
  echo "错误：找不到 pio，请先安装 PlatformIO 并确保 pio 在 PATH 中。" >&2
  exit 1
fi

if [[ $# -gt 1 ]]; then
  echo "用法：$0 [串口路径]" >&2
  exit 2
fi

if [[ $# -eq 1 ]]; then
  UPLOAD_PORT="$1"
else
  PORTS=(/dev/cu.usbmodem*)
  if [[ ! -e "${PORTS[0]}" ]]; then
    echo "错误：没有找到 /dev/cu.usbmodem*，请连接 ESP32-C3 或手动指定串口。" >&2
    echo "示例：$0 /dev/cu.usbmodem11201" >&2
    exit 1
  fi
  if [[ ${#PORTS[@]} -gt 1 ]]; then
    echo "检测到多个 USB 串口，请手动指定其中一个：" >&2
    printf '  %s\n' "${PORTS[@]}" >&2
    exit 1
  fi
  UPLOAD_PORT="${PORTS[0]}"
fi

if [[ ! -e "${UPLOAD_PORT}" ]]; then
  echo "错误：串口不存在：${UPLOAD_PORT}" >&2
  exit 1
fi

echo "使用串口：${UPLOAD_PORT}"
echo "[1/3] 编译固件..."
pio run --project-dir "${PROJECT_DIR}"

echo "[2/3] 上传固件..."
pio run --project-dir "${PROJECT_DIR}" --target upload --upload-port "${UPLOAD_PORT}"

echo "[3/3] 上传 LittleFS 网页资源..."
pio run --project-dir "${PROJECT_DIR}" --target uploadfs --upload-port "${UPLOAD_PORT}"

echo "烧录完成。"
