#!/bin/bash
# nvmtl-boost.sh off|on : NullMoth NVMTL holds every Metal process's GPU at max clocks (PERF_BOOST INFINITE, knob
# BOOST in /Library/GPUBundles/nvmtl/nvrm610.conf, default on). "off" writes BOOST=0 into the installer-kept
# "my settings" block, so the GPU uses its own clock control and the Fn+Q watcher (local.nvclk) holds the boost
# only in red mode. "on" removes the line again. Backup next to the file first. Takes effect for processes started
# afterwards: reboot.
set -euo pipefail
C=/Library/GPUBundles/nvmtl/nvrm610.conf
S='# --- my settings, kept on update ---'; E='# --- end my settings ---'
[ "$(id -u)" = 0 ] || { echo "СТОП: потрібен sudo"; exit 1; }
[ -f "$C" ] || { echo "СТОП: немає $C"; exit 1; }
case ${1:-} in
  off)
    grep -qx 'BOOST=0' "$C" && { echo "Вже BOOST=0. Нічого не роблю."; exit 0; }
    cp -p "$C" "$C.bak-boost-$(date +%Y%m%d-%H%M%S)"
    if grep -qxF "$S" "$C"; then
      awk -v e="$E" '$0==e{print "BOOST=0"} {print}' "$C" > "$C.tmp"
    else
      { cat "$C"; printf '%s\nBOOST=0\n%s\n' "$S" "$E"; } > "$C.tmp"
    fi
    ;;
  on)
    grep -qx 'BOOST=0' "$C" || { echo "BOOST=0 немає. Нічого не роблю."; exit 0; }
    cp -p "$C" "$C.bak-boost-$(date +%Y%m%d-%H%M%S)"
    grep -vx 'BOOST=0' "$C" > "$C.tmp"
    ;;
  *) echo "використання: sudo bash $0 off|on"; exit 2;;
esac
chown root:wheel "$C.tmp"; chmod 644 "$C.tmp"; mv "$C.tmp" "$C"
echo "ГОТОВО ($1). Зараз у $C:"; cat "$C"
echo "Перезавантаж, щоб зміна діяла."
