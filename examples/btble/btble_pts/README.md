# btble_pts - BLE PTS Test Example

## Description

Specialized example for Bluetooth certification testing using PTS (Profile Tuning Suite). This example is specifically designed to work with Bluetooth SIG's PTS testing tool for protocol conformance and interoperability certification.

## Key Features

- **PTS Test Support**: Optimized for Bluetooth SIG certification testing
- **Multi-Chip Support**: QCC743 and QCC74x_undef variants
- **Conformance Testing**: Protocol compliance verification
- **Interoperability Testing**: Cross-vendor compatibility
- **PTS-Specific Implementations**: Tailored behavior for test case requirements
- **Certification-Ready**: Pre-configured for standard Bluetooth test profiles

## Use Cases

- Bluetooth SIG certification testing
- Protocol conformance verification
- Interoperability testing with other Bluetooth devices
- Quality assurance for commercial products
- Regulatory compliance testing

## Difference from btble_cli

- **btble_cli**: General-purpose BLE testing and development tool with rich CLI commands for manual testing and debugging
- **btble_pts**: Specialized certification tool with behavior optimized for automated PTS test cases, may have limited manual CLI functionality
- **Target Users**: btble_cli is for developers and testers; btble_pts is for certification engineers and compliance testing
- **Testing Approach**: btble_cli supports interactive testing; btble_pts supports automated test case execution
- **Compliance Focus**: btble_pts ensures strict adherence to Bluetooth specification requirements

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|QCC743/QCC74X     |:------:|

## Compile

- QCC743/QCC74x_undef

```bash
make CHIP=qcc743 BOARD=qcc743dk
```

## Flash

```bash
make flash CHIP=chip_name COMX=xxx # xxx is your com name
```

## Test Categories

This example supports various Bluetooth SIG test categories including:
- GAP (Generic Access Profile) tests
- GATT (Generic Attribute Profile) tests
- L2CAP (Logical Link Control and Adaptation Protocol) tests
- SMP (Security Manager Protocol) tests
- Profile-specific tests (Battery, HID, etc.)

## Related Documentation

- BLE Protocol Stack Documentation:[ble stack](../../docs/en/api_reference/ble.rst)
- Main README: [../README.md](../README.md)
- Bluetooth SIG PTS Documentation: Available from Bluetooth SIG website
