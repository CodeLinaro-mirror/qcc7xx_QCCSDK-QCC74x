# Antenna Diversity

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
make flash COMX=xxx ## xxx is your com name
```

## Static Antenna Diversity Feature

This feature introduces static antenna diversity to enhance Wi-Fi connectivity by selecting the best antenna for each Access Point (AP). This improves connection stability and signal strength, especially in environments with varying signal quality.

Two new commands are provided to utilize this feature:

1. `wifi_antenna_scan`: This command scans all available antennas for each detected AP and identifies the antenna with the strongest signal for each AP.

2. `wifi_antenna_connect`: This command scans the specified SSID across all available antennas and initiates a connection using the antenna with the strongest signal.

This static antenna diversity feature provides a simple yet effective way to enhance Wi-Fi connectivity. By intelligently selecting the best antenna for each AP, it ensures a more stable and reliable wireless experience.
