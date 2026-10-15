# UART WiFi Configuration and Usage

## UART1 Pin Configuration

The AT command channel uses UART1 at 2000000 baud, 8 data bits, 1 stop bit,
and no parity. The following tables list the QCC74x pins used by each build
configuration.

### Default UART1 Pins

If you are using the default QCC743/QCC744 development board configuration,
UART1 uses the following pins:

|  Signal  | QCC74x |
|----------|--------|
| UART1 TX | GPIO27 |
| UART1 RX | GPIO28 |
| UART1 CTS | GPIO29 |
| UART1 RTS | GPIO30 |
|  Ground  |  GND   |

### GPIO14/15 Overlay Pins

To use GPIO14 and GPIO15 instead, build the firmware with the demo-specific
GPIO overlay enabled:

```bash
make CONFIG_UART_WIFI_GPIO_OVERLAY=y
```

UART1 then uses the following pins:

|  Signal  | QCC74x |
|----------|--------|
| UART1 TX | GPIO14 |
| UART1 RX | GPIO15 |
|  Ground  |  GND   |

Without `CONFIG_UART_WIFI_GPIO_OVERLAY=y`, the firmware continues to use the
default GPIO27-GPIO30 UART1 pin configuration.

# Low Power Mode Configuration and Usage

## Configuration

To enable low power mode, you need to set the `CONFIG_USE_LPAPP` variable in your configuration. This can be done by uncommenting or adding the following line in your project's configuration file:

```bash
CONFIG_USE_LPAPP =y
```

After making the necessary changes, you need to recompile your project:

```bash
make
```

## Flashing

To flash your project onto the target device, use the following command, where `xxx` is your serial port name:

```bash
make flash COMX=xxx # xxx is your com port name
```

## How to Use PWR Mode

### PWR Mode Commands

Reference: atmoudle.rst

Supported commands for PWR mode are:

- AT+PWR
- AT+DTIM

#### AT+PWR Command

Command Format:

```bash
AT+PWR=<power mode>[,<timeout>][,<level>]
```

Response:

```bash
OK
```

Parameters:

- `<power mode>`:
    - 0: normal mode.
    - 1: hibemate mode.
    - 2: standby mode.

- `<timeout>`: Hold time in low-power mode, in milliseconds.
- `[<level>]`: In hibemate mode, power level ranging from 0 to 2.

#### Examples

```bash
AT+PWR=0                       // Normal mode
AT+PWR=1,1000,0                // Hibemate mode, timeout 1000ms, level 0.
AT+PWR=2,30000000              // Standby mode, timeout 30000000 ms
```

#### AT+SLWKDTIM Command

To set the wakeup DTIM value:

- `<dtim>`: Set the wakeup DTIM.

Example:

```bash
AT+SLWKDTIM=10                      // Set DTIM 10 for wakeup.
```

### Testing PWR Mode

#### Testing PWR Step

1. Use a power analyzer to supply power to the demo board.
2. Reset the demo board.
3. Execute the command `AT+PWR=1,1000,0`
4. You can see that the current has decreased, and then measure the subsequent average power consumption.

#### Testing DTIM Step

1. Use a power analyzer to supply power to the demo board.
2. Reset the demo board.
3. Connect to the Access Point (AP).
4. Send `AT+SLWKDTIM=10`
5. Send `AT+PWR=2,30000` (enter standby mode)
6. You can see that the current has decreased, and then measure the subsequent average power consumption.

### NOTE

If the board is not restarted after exiting due to low power timeout, execute `'spisync_wakeup'` on the Device Under Test (DUT) side first. Then, you can continue using the above commands to test low power.
