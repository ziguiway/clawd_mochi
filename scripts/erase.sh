#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if ! command -v pio >/dev/null 2>&1; then
  echo "错误：找不到 pio，请先安装 PlatformIO 并确保 pio 在 PATH 中。" >&2
  exit 1
fi

if [[ $# -gt 2 ]]; then
  echo "用法：$0 [串口路径] [--yes]" >&2
  exit 2
fi

UPLOAD_PORT=""
CONFIRMED=false
for argument in "$@"; do
  case "${argument}" in
    --yes) CONFIRMED=true ;;
    /dev/*|COM[0-9]*)
      if [[ -n "${UPLOAD_PORT}" ]]; then
        echo "错误：只能指定一个串口。" >&2
        exit 2
      fi
      UPLOAD_PORT="${argument}"
      ;;
    *)
      echo "错误：未知参数：${argument}" >&2
      echo "用法：$0 [串口路径] [--yes]" >&2
      exit 2
      ;;
  esac
done

if [[ -z "${UPLOAD_PORT}" ]]; then
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

echo "警告：将擦除 ${UPLOAD_PORT} 的完整 Flash。"
echo "固件、LittleFS、WiFi 凭据、NVS 和所有配置都会被删除。"
if [[ "${CONFIRMED}" != true ]]; then
  read -r -p "确认继续？输入 ERASE：" confirmation
  if [[ "${confirmation}" != "ERASE" ]]; then
    echo "已取消。"
    exit 1
  fi
fi

echo "使用串口：${UPLOAD_PORT}"
pio run --project-dir "${PROJECT_DIR}" \
  --target erase --upload-port "${UPLOAD_PORT}"

echo "全片擦除完成。现在可以执行 scripts/flash.sh 重新烧录。"
