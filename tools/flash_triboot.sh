#!/usr/bin/env bash
# ============================================================================
#  EFIRNET UNIWARE V.1 — заливка платы с компьютера
#
#  Запуск:  ./flash_triboot.sh <плата> <порт>
#  Пример:  ./flash_triboot.sh heltec_v3 COM6
#           ./flash_triboot.sh                 # покажет список плат
#
#  Берёт ровно те же образы, что уходят в онлайн-прошивку, — из каталогов
#  сборки трёх проектов. Чего нет на диске, то молча пропускается, так что
#  этим же скриптом можно перезалить один слот.
#
#  В конце otadata стирается: пустой otadata = плата поднимется в меню.
# ============================================================================
set -euo pipefail

BOARD="${1:-}"
PORT="${2:-}"
BAUD=921600
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLAUDE="$(cd "$ROOT/.." && pwd)"

# плата|мб|s3|env менеджера|env ЭФИРНЕТа|env Meshtastic|env MeshCore
BOARDS="
heltec_v3|8|1|heltec_v3|heltec_v3_uniware|heltec-v3-uniware|Heltec_v3_uniware
heltec_stick_lite_v3|8|1|heltec_stick_lite_v3|heltec_stick_lite_v3_uniware|heltec-wsl-v3-uniware|Heltec_WSL3_uniware
lilygo_t3s3|8|1|lilygo_t3s3|lilygo_t3s3_uniware|tlora-t3s3-v1-uniware|LilyGo_T3S3_uniware
heltec_v2|8|0|heltec_v2|heltec_v2_uniware|heltec-v2_1-uniware|Heltec_v2_uniware
heltec_v4|16|1|heltec_v4|heltec_v4_uniware|heltec-v4-uniware|heltec_v4_uniware
heltec_v4_r8|16|1|heltec_v4_r8|heltec_v4_r8_uniware|hv4r8-uniware|heltec_v4_r8_uniware
"

row="$(echo "$BOARDS" | grep "^${BOARD}|" || true)"
if [ -z "$BOARD" ] || [ -z "$PORT" ] || [ -z "$row" ]; then
    echo "Использование: $0 <плата> <порт>"
    echo "Платы:"
    echo "$BOARDS" | grep -v '^$' | cut -d'|' -f1 | sed 's/^/  /'
    exit 1
fi

IFS='|' read -r _ MB S3 ENV_MGR ENV_EFR ENV_MT ENV_MC <<< "$row"

if [ "$MB" = "16" ]; then
    OFF_APP_MT=0x200000; OFF_APP_MC=0x600000
else
    OFF_APP_MT=0x160000; OFF_APP_MC=0x400000
fi
if [ "$S3" = "1" ]; then CHIP=esp32s3; OFF_BOOTLOADER=0x0; else CHIP=esp32; OFF_BOOTLOADER=0x1000; fi
OFF_PARTITIONS=0x8000
OFF_OTADATA=0xe000
OFF_FACTORY=0x10000
OFF_APP_EFR=0x80000

PYPIO="$HOME/.platformio/penv/Scripts/python.exe"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
[ -x "$PYPIO" ] || PYPIO=python
esp() { PYTHONPATH="$(dirname "$ESPTOOL")" "$PYPIO" "$ESPTOOL" "$@"; }

MGRDIR="$ROOT/bootmanager/.pio/build/$ENV_MGR"
EFRDIR="$CLAUDE/efirnet/firmware/.pio/build/$ENV_EFR"
MTDIR="$CLAUDE/meshtastic-src/.pio/build/$ENV_MT"
MCDIR="$CLAUDE/meshcore-src/.pio/build/$ENV_MC"

# Meshtastic переименовывает свой образ в firmware-<вариант>-<версия>.bin.
# .factory.bin брать нельзя: в нём внутри свой загрузчик и своя таблица разделов.
mt_bin="$(ls "$MTDIR"/firmware*.bin 2>/dev/null | grep -v factory | head -1 || true)"

ARGS=()
add() {  # add <offset> <file> <подпись>
    if [ -n "$2" ] && [ -f "$2" ]; then
        printf '  %-11s %-9s %8d B  %s\n' "$3" "$1" "$(stat -c%s "$2")" "$(basename "$2")"
        ARGS+=("$1" "$2")
    else
        printf '  %-11s %-9s ПРОПУСК (нет файла)\n' "$3" "$1"
    fi
}

echo "Плата $BOARD ($CHIP, $MB МБ), порт $PORT. Будет залито:"
add $OFF_BOOTLOADER "$MGRDIR/bootloader.bin" bootloader
add $OFF_PARTITIONS "$MGRDIR/partitions.bin" parttable
add $OFF_FACTORY    "$MGRDIR/firmware.bin"   uniware
add $OFF_APP_EFR    "$EFRDIR/firmware.bin"   efirnet
add $OFF_APP_MT     "$mt_bin"                meshtastic
add $OFF_APP_MC     "$MCDIR/firmware.bin"    meshcore
echo

# --flash_mode/-freq/-size keep: у каждого образа в заголовке уже стоят
# значения, с которыми его собирали. Переписывать их своими — верный способ
# получить плату, которая не выходит из ребута.
esp --chip $CHIP --port "$PORT" --baud $BAUD \
    write_flash -z --flash_mode keep --flash_freq keep --flash_size keep "${ARGS[@]}"

echo
echo "Стираю otadata -> плата загрузится в меню"
esp --chip $CHIP --port "$PORT" --baud $BAUD erase_region $OFF_OTADATA 0x2000

echo "Готово."
