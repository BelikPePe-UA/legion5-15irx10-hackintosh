# NVRM: GPS _DSM і cTGP за режимом Fn+Q (експеримент)

Патчі для NullMoth 2.4.3 NVRM (open-gpu-kernel-modules 610.57.04):

| Патч | Файл | Що робить |
|---|---|---|
| `610-gps-dsm-passthrough.patch` | NullMoth `kexts/NVRM/os-xnu2.cpp` | Пропускає GPS `_DSM` до BIOS (boot-arg `nvrmgps=1`). З `nvrmgps=2` додає cTGP за режимом Lenovo (0 / +15 / +50 Вт), а на Fn+Q (FnQNotify) надсилає RM подію «стан GPS змінився» |
| `610-gps-ctgp-runtime-rm.patch` | RM `platform_request_handler_ctrl.c` | Перезастосовує cTGP не лише під час запуску, а й на кожну таку подію |
| `610-gps-status-change-rm.patch` | RM `arch/nvalloc/unix/src/osapi.c` | Подія ACPI GPS іде в `PFM_REQ_HNDLR_CONTROL`, бо `GPS_CONTROL` у цьому RM є заглушкою |

Результат: ліміт доходить до GPU на кожне Fn+Q, але продуктивність не змінюється. У macOS відеокарта не впирається в ліміт потужності. Реально керувати картою через Fn+Q вдалося іншим способом: [tools/nvclk](../../tools/nvclk).

`os-xnu2.cpp` є частиною NullMoth і поширюється за PolyForm Noncommercial License 1.0.0 (© NullMoth). Патч до нього можна використовувати лише некомерційно. RM-патчі зроблені до коду NVIDIA open-gpu-kernel-modules (MIT/GPLv2).
