## RCP Host Low Power Wakeup

`CONFIG_SDIO_OOB` selects the host-to-device wakeup line used by the SDIO
transport.

Device-side low-power command details are documented in
[`examples/wifi/nethub_sdio_wifi/README.md`](../wifi/nethub_sdio_wifi/README.md).

### CONFIG_SDIO_OOB=0 (Default)

This is the default in-band SDIO wakeup mode. The host reuses SDIO `D2` /
`GPIOC PIN10` as the wake line. During wakeup, the host temporarily switches this
pin from `GPIO_AF12_SDMMC1` to GPIO output and drives it low, then restores it to
the SDMMC1 alternate function.

Structure:

```text
Host STM32                              Device qcc743
-----------                             -------------
PC10 / SDIO D2  ---- drive low -------> GPIO10 / SDIO DAT2
                                        wakes by WIFI_WKIO 10 1

SDIO DAT1 IRQ   <--- active low ------- GPIO15 / SDIO DAT1

PC10 / SDIO D2  ---- restore SDIO AF -> GPIO10 sees ready release
SDIO CMD/CLK/DAT[0:3] <=============> SDIO CMD/CLK/DAT[0:3]
```

Build with make:

```sh
cd STM32CubeIDE/Appli/Debug
make all
make all CONFIG_SDIO_OOB=0
```

Build with STM32CubeIDE:

No extra preprocessor symbol is required. If `CONFIG_SDIO_OOB` was added before,
remove it or set it to `0`.

Use from the STM32 host shell:

```text
WIFI_WKIO 10 1
wifi_connect <ssid> <passw>
WIFI_PWR 2 1
```

### CONFIG_SDIO_OOB=1

This is the dedicated OOB wakeup mode. The host uses `GPIOD PIN14`, defined by
`QCC74X_OOB_WAKEUP_Pin` and `QCC74X_OOB_WAKEUP_GPIO_Port` in
`Appli/Inc/rcp_host_config.h`, as the wake line.

The dedicated OOB GPIO defaults are:

```c
#define QCC74X_OOB_WAKEUP_Pin GPIO_PIN_14
#define QCC74X_OOB_WAKEUP_GPIO_Port GPIOD
```

Structure:

```text
Host STM32                              Device qcc743
-----------                             -------------
PD14 / OOB_WAKEUP ---- drive high ----> GPIO28
                                        wakes by WIFI_WKIO 28 0

SDIO DAT1 IRQ      <--- active low ---- GPIO15 / SDIO DAT1

PD14 / OOB_WAKEUP ---- drive low -----> GPIO28 sees ready release
SDIO CMD/CLK/DAT[0:3] <=============> SDIO CMD/CLK/DAT[0:3]
```

Build with make:

```sh
cd STM32CubeIDE/Appli/Debug
make all CONFIG_SDIO_OOB=1
```

Build with STM32CubeIDE:

Open the `STM32CubeIDE/Appli` project, then add this preprocessor symbol:

```text
CONFIG_SDIO_OOB=1
```

CubeIDE menu path:

```text
Project Properties
  -> C/C++ Build
  -> Settings
  -> MCU GCC Compiler
  -> Preprocessor
  -> Defined symbols (-D)
```

Add `CONFIG_SDIO_OOB=1`, then rebuild the application project.

Use from the STM32 host shell:

```text
WIFI_WKIO 28 0
wifi_connect <ssid> <passw>
WIFI_PWR 2 1
```

### Generated Config Header

`STM32CubeIDE/Appli/makefile.defs` generates `Appli/Inc/rcp_host_config.h` with
a default value only when `CONFIG_SDIO_OOB` was not already provided by the
compiler command line. This keeps the CubeIDE preprocessor setting from being
overwritten by the generated header.

`CONFIG_SDIO_OOB=1`, `CONFIG_SDIO_OOB=y`, and `CONFIG_SDIO_OOB=Y` are treated as
enabled by `STM32CubeIDE/Appli/makefile.defs`; other values are treated as
disabled.

The make flow rewrites `Appli/Inc/rcp_host_config.h` before compilation. The
generated `CONFIG_SDIO_OOB` default is wrapped as follows:

```c
#ifndef CONFIG_SDIO_OOB
#define CONFIG_SDIO_OOB 0
#endif
```

This means the effective value is:

1. CubeIDE/compiler `-DCONFIG_SDIO_OOB=...`, if provided.
2. Otherwise the value generated from `make CONFIG_SDIO_OOB=...`.
3. Otherwise the default value `0`.
