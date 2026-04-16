# Nethub SDIO WiFi Example

## Overview

`nethub_sdio_wifi` is the qcc743 device-side example for the Nethub SDIO RCP
solution.

In this solution:

- The qcc743 acts as the SDIO RCP device
- The host acts as the SDIO RCP master
- Control and data exchange are both carried over the SDIO-based RCP link
- The host side provides Wi-Fi control, network bring-up, and packet handling on
  top of the RCP transport

This example is intended to work with the STM32 host reference implementation in:

`components/net/nethub/host/stm32_host`

## Features

- qcc743 SDIO2 RCP device implementation
- Nethub forwarding over the SDIO RCP transport
- STM32 host reference support
- Host-side Wi-Fi connection management
- Host-side automatic DHCP after Wi-Fi association
- Host-side TCP and UDP iperf validation commands

## Supported Platform

- Chip: `qcc743`
- Device interface: `sdio2`

## Build

From the example directory:

```bash
make
```

The generated firmware is located at:

- `build/build_out/nethub_sdio_wifi_qcc743.elf`
- `build/build_out/nethub_sdio_wifi_qcc743.bin`

## Flash

Example:

```bash
make flash COMX=/dev/ttyUSB0
```

Adjust `COMX` to match the serial port used by your board.

## qcc743 SDIO2 Pin Mapping

The qcc743 SDIO2 test configuration in the SDK uses the following pins:

| Signal | qcc743 GPIO |
|:------:|:-----------:|
| DAT2   | GPIO10 |
| DAT3   | GPIO11 |
| CMD    | GPIO12 |
| CLK    | GPIO13 |
| DAT0   | GPIO14 |
| DAT1   | GPIO15 |

Reference implementation:

- `examples/peripherals/sdio/sdio2_test/main.c`

## Device-Side Startup

After boot, the device performs:

1. RF parameter initialization
2. TCP/IP stack initialization
3. Wi-Fi clock enable and reset
4. Wi-Fi IRQ registration
5. EasyFlash initialization
6. Nethub SDIO Wi-Fi initialization
7. Control service startup for the SDIO RCP host

Reference files:

- `main.c`
- `app/src/app_user.c`

## Host-Side Reference

### Host Project

The current reference host is the STM32 host project:

- `examples/stm32_host`

Host build directory:

- `examples/stm32_host/STM32CubeIDE/Appli/Debug`

Example host build command:

```bash
make -C examples/stm32_host/STM32CubeIDE/Appli/Debug all -j4
```

### Host Role

The STM32 host is the SDIO RCP master. It is responsible for:

- SDIO link initialization and handshake
- RCP stream setup
- Control path processing
- Host-side LwIP integration
- DHCP start and stop
- Optional iperf verification through the shell

### Host Shell

The STM32 host exposes a UART shell on `USART3`.

The shell is mainly a debug and bring-up interface for the host side.

Available Wi-Fi commands include:

- `wifi_connect <SSID> [PSK]`
- `wifi_disconnect`

Available iperf shell commands include:

- `iperf_c <server_ip>` for TCP TX
- `iperf_s` for TCP RX
- `iperf_u_c <server_ip>` for UDP TX
- `iperf_u_s` for UDP RX
- `iperf_stop`

### Wi-Fi Connection Flow

Typical host-side workflow:

1. Power on the qcc743 device and the STM32 host
2. Wait until the SDIO link is initialized
3. Open the STM32 host shell on `USART3`
4. Run:

```text
wifi_connect <SSID> [PSK]
```

For example:

```text
wifi_connect MyAP 12345678
```

From the system point of view, this is a host-driven SDIO RCP workflow:

- The host sends a Wi-Fi connect request over the RCP control path
- The qcc743 executes the Wi-Fi operation on the device side
- The host observes the connection state and brings up host networking

The exact internal control payload format is not the primary usage concern for
this README. The important point is that the user-facing workflow is based on the
SDIO RCP solution, not on a standalone UART Wi-Fi module model.

### Automatic DHCP

The current STM32 host implementation starts DHCP automatically on the host side.

Behavior:

- The host monitors the connection state reported by the device through the RCP
  control path
- After the device reports Wi-Fi connected, the host starts DHCP automatically
- After the device reports Wi-Fi disconnected, the host stops the DHCP flow

This means:

- No manual DHCP command is required on the host shell
- No static IP configuration is required on the STM32 host for the normal STA workflow

Relevant implementation:

- `components/net/nethub/host/stm32_host/RCP/Src/rcp_wifi_mgmt.c`
- `components/net/nethub/host/stm32_host/RCP/Src/rcp_lwip_netif.c`

## System Notes

- This example should be understood as an SDIO RCP solution
- The qcc743 side provides the SDIO RCP device, Wi-Fi capability, and Nethub forwarding
- The STM32 side provides the SDIO RCP host, shell, host networking, and DHCP behavior
- Wi-Fi association is host-driven through the RCP control workflow
- The host currently starts DHCP after the device reports Wi-Fi connected

## Known Issues

- Automatic hot-plug is not supported
- If the qcc743 device is reset, the host must also be reset to complete the SDIO handshake again
- After a device reset, the current host implementation cannot recover the link automatically without a host reboot or reset
- In other words, automatic host-side re-handshake after device reset is not supported yet

## Related Paths

- Device example: `examples/wifi/nethub_sdio_wifi`
- Host reference: `components/net/nethub/host/stm32_host`
- SDIO transport: `components/net/nethub/transportsdio`
- Nethub core: `components/net/nethub/nethub`

## License

MIT License
