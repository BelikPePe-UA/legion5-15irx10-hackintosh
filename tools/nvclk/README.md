# nvclk: частоти RTX 5070 і режими Fn+Q для відеокарти (NullMoth)

`nvclk` відкриває драйвер NullMoth так само, як це робить NVK (IOKit user client `NVAccel`/`NVRM`), і надсилає йому звичайні команди RM з відкритого драйвера NVIDIA. Права адміністратора не потрібні.

| Команда | Що робить |
|---|---|
| `nvclk [секунд]` | Тільки читає: частота ядра (gpc) і пам'яті, P-state, раз на секунду |
| `nvclk tdp get` | Показує поточне обмеження частоти (RATED_TDP, клієнт GLOBAL) |
| `nvclk tdp limit` | Обмежує частоту: `RATED_TDP` → ~900 МГц, з `NVCLK_TURBO=1` → `TURBO_BOOST` ~1417 МГц |
| `nvclk tdp default` | Знімає обмеження |
| `nvclk watch` | Служба: стежить за Fn+Q (через FnQNotify.kext) і перемикає відеокарту |

Режими `watch`:

| Fn+Q | Відеокарта |
|---|---|
| 🔵 Тихий | власне керування частотою + стеля `TURBO_BOOST` (1417 МГц) |
| ⚪ Збалансований | власне керування частотою: у простої скидає, під навантаженням піднімає |
| 🔴 Продуктивність | `PERF_BOOST` до максимуму, тримає процес `nvclk` |

## Встановлення

1. Зберіть: `./build.sh ~/src/open-gpu-kernel-modules` (тег `610.57.04`, на ньому зібрано NullMoth 2.4.3).
2. `sudo cp nvclk /usr/local/bin/`
3. Вимкніть постійний буст NVMTL, щоб працювали тихий і збалансований режими: `sudo bash nvmtl-boost.sh off`, потім перезавантаження. Повернути: `sudo bash nvmtl-boost.sh on`.
4. Служба в автозапуску: `cp local.nvclk.plist ~/Library/LaunchAgents/ && launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/local.nvclk.plist`.
5. Перевірка: натисніть Fn+Q, подивіться `/tmp/nvclk-watch.log` і `nvclk 3`.

Вимкнути: `launchctl bootout gui/$(id -u)/local.nvclk; nvclk tdp default`.

## Що вийшло (22.10.2026)

| Режим | Простій | Навантаження | FP32 (gpu-sustain) |
|---|---|---|---|
| Тихий | ~790 МГц | 1417 МГц | 5603 GFLOPS |
| Збалансований | ~1340 МГц | ~2270 МГц | 9564 GFLOPS |
| Продуктивність | ~2090 МГц | ~2240 МГц | 9490 GFLOPS |

`render-modes.swift` малює з цих чисел графік для статті.
