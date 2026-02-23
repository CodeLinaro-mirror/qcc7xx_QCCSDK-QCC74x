# btble_cli - BLE Command Line Interface Example

## Description

Provides a comprehensive BLE command line interface for testing and development. This example includes complete BLE protocol stack operations with extensive command support for manual testing, debugging, and development of BLE applications.

## Key Features

- BLE controller and host stack initialization
- Rich serial command line interface with BLE CLI commands
- Connection state callback handling
- Auto-restart advertising functionality
- Support for various BLE operations (scan, connect, GATT operations, etc.)

## Use Cases

- BLE functionality verification and testing
- Protocol stack development and debugging
- Performance evaluation
- Educational purposes for BLE protocol learning

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|qcc743/qcc744       |        |

## Compile

- qcc743/qcc744

```bash
make CHIP=qcc743 BOARD=qcc743dk
```

## Flash

```bash
make flash CHIP=chip_name COMX=xxx # xxx is your com name
```

## Related Documentation

- BLE Protocol Stack Documentation:[ble stack](../../docs/en/api_reference/ble.rst)
- Main README: [../README.md](../README.md)
