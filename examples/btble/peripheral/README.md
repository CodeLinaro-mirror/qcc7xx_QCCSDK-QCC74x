# Peripheral

This sample description ble peripheral function with READ, WRITE/WRITE NO RESPONSE, INDICATE and NOTIFY properties. Can be linked and communicated by ble app.

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|qcc743/qcc744       |        |

## Compile

- qcc743/qcc744

```
make CHIP=qcc743 BOARD=qcc743dk
```

## Flash

```
make flash CHIP=chip_name COMX=xxx # xxx is your com name
```

## Generate BlE OTA file

```
python components/wireless/bluetooth/tools/GenerateOAD.py FW.bin FW.bin.sha256
```
## 