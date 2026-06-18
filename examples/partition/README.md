# Flash Partition Configuration and Usage Guide

## Supported Chips

| CHIP | Remarks |
|:----:|:--------:|
| QCC743 / QCC744 | Supported by default |

---

## Compilation

Example for QCC743 / QCC744:

```bash
make CHIP=qcc743 BOARD=qcc743dk
```

---

## Flashing Firmware

To flash the firmware to your board:

```bash
make flash CHIP=<chip_name> COMX=<port_name>  # Replace <port_name> with your serial port (e.g., COM5 or /dev/ttyUSB0)
```

---

## Default Partition Table

Without partition table setting, the system will use the **default 4MB partition configuration** located at:

```
`sdk/bsp/qcc74xdk/config/partition_cfg_4M.toml`

```

### `partition_cfg_4M.toml`:

```toml
[pt_table]
#partition table is 4K in size
address0 = 0xE000
address1 = 0xF000
# If version is 2, it will use dynamic mode.
version = 2

[[pt_entry]]
type = 16
name = "Boot2"
device = 0
address0 = 0
size0 = 0xE000
address1 = 0
size1 = 0
# compressed image must set len,normal image can left it to 0
len = 0
# If header is 1, it will add the header.
header = 1
# If header is 1 and security is 1, It will be encrypted.
security = 1

[[pt_entry]]
type = 0
name = "FW"
device = 0
address0 = 0x10000
size0 = 0x1B4000
address1 = 0x1C4000
size1 = 0x1B4000
# compressed image must set len,normal image can left it to 0
len = 0
# If header is 1, it will add the header.
header = 1
# If header is 1 and security is 1, It will be encrypted.
security= 1

[[pt_entry]]
type = 10
name = "mfg"
device = 0
address0 = 0x1C4000
size0 = 0x1B4000
address1 = 0
size1 = 0
# compressed image must set len,normal image can left it to 0
len = 0
# If header is 1, it will add the header.
header = 1
# If header is 1 and security is 1, It will be encrypted.
security= 1
...
```

### Partition entries:
   - **type**: Partition type.Boot2 uses types `0` and `1` to load the main firmware, so these types should never be reused for other purposes. 
   - **device**: Reserved (unused). 
   - **name**: Partition name (up to 8 characters). 
   - **address0**: Start address of the primary partition. 
   - **size0**: Maximum size of the primary partition. 
   - **address1**: Start address of the backup partition (if applicable). 
   - **size1**: Maximum size of the backup partition. 
   - **len**: 0 = uncompressed, 1 = compressed image. 
   - **header**: 0 = no header, 1 = add header for boot. 
   - **security**: 0 = non-secure, 1 = secure (encrypted). 
   If `header=1` and `security=1`, this image will be encrypted and add 4K boot header during flashing. 

---

### Flash Address Map

   `make flash ...` will flash firmware and other content into the flash memory based on the partition file. The default flash layout is:

   | Name | Type | Start Address | Size (Bytes) | Backup Addr | Backup Size | Header | Security |
   |------|------|----------------|--------------|--------------|--------------|---------|-----------|
   | Boot2   | 16 | 0x00000000 | 57344 (56KB) | 0 | 0 | 1 | 1 |
   | Pt_table0 |   | 0x0000E000 | 4096 (4KB) | 0 | 0 | 1 | 1 |
   | Pt_table1 |   | 0x0000F000 | 4096 (4KB) | 0 | 0 | 1 | 1 |
   | FW      | 0  | 0x00010000 | 1785856 (1744KB) | 0x001C4000 | 1785856 (1744KB) | 1 | 1 |
   | mfg     | 10 | 0x001C4000 | 1785856 (1744KB) | 0 | 0 | 1 | 1 |
   | PSM     | 3  | 0x00378000 | 32768 (32KB) | 0 | 0 | 0 | 0 |
   | media   | 4  | 0x00380000 | 32768 (32KB) | 0 | 0 | 0 | 0 |
   | rsvd    | 2  | 0x00388000 | 483328 (472KB) | 0 | 0 | 0 | 0 |
   | MFD     | 5  | 0x003FE000 | 4096 (4KB) | 0 | 0 | 0 | 0 |
   | FACTORY | 8  | 0x003FF000 | 4096 (4KB) | 0 | 0 | 0 | 0 |

---

### Partition Descriptions

- **Boot2** – Secondary bootloader that initializes the system and loads the main firmware.  
- **Partition Table 0/1** – Redundant partition tables; Boot2 selects the valid one at startup.  
- **FW** – Main firmware image, with backup area for OTA.  
- **mfg** – Factory or manufacturing firmware section. This partition occupies the same flash region as the FW backup partition (FW's address1/size1). 

- **media** – Partition used for BLE/Wi-Fi/Zigbee or other stack-related applications data.  
- **rsvd** – Reserved partition for application use.  
- **PSM** – Parameter Storage Manager partition (used by LittleFS).  
- **MFD** – Matter Factory Data partition.  
- **FACTORY** – Factory data and calibration area.  

---

## Configuration a Custom Partition

To add a new partition (e.g., a key-value partition), you should arrange the 4M flash mapping, then create a new `.toml` file in your project directory and set the override variable in your configuration file:


### Updated Layout Example

   | Name | Type | Start Address | Size (Bytes) | Backup Addr | Backup Size | Header | Security |
   |------|------|----------------|--------------|--------------|--------------|---------|-----------|
   | Boot2   | 16 | 0x00000000 | 57344 (56KB) | 0 | 0 | 1 | 1 |
   | Pt_table0 |   | 0x0000E000 | 4096 (4KB) | 0 | 0 | 1 | 1 |
   | Pt_table1 |   | 0x0000F000 | 4096 (4KB) | 0 | 0 | 1 | 1 |
   | FW      | 0  | 0x00010000 | 1785856 (1744KB) | 0x001C4000 | 1785856 (1744KB) | 1 | 1 |
   | mfg     | 10 | 0x001C4000 | 1785856 (1744KB) | 0 | 0 | 1 | 1 |
   | PSM     | 3  | 0x00378000 | 32768 (32KB) | 0 | 0 | 0 | 0 |
   | media   | 4  | 0x00380000 | 32768 (32KB) | 0 | 0 | 0 | 0 |
   | rsvd    | 2  | 0x00388000 | 479232 (468KB) | 0 | 0 | 0 | 0 |
   | kv      | 5  | 0x003FD000 | 4096 (4KB) | 0 | 0 | 0 | 0 |
   | MFD     | 5  | 0x003FE000 | 4096 (4KB) | 0 | 0 | 0 | 0 |
   | FACTORY | 8  | 0x003FF000 | 4096 (4KB) | 0 | 0 | 0 | 0 |

---
### Updated Layout Example

```toml


[[pt_entry]]
type = 2
name = "rsvd"
device = 0
address0 = 0x388000
size0 = 0x75000
address1 = 0
size1 = 0
len = 0
header = 0
security= 0

[[pt_entry]]
type = 5
name = "kv"
device = 0
address0 = 0x3FD000
size0 = 0x1000
address1 = 0
size1 = 0
len = 0


```

## Important Tips

- Align partitions to **4KB flash sector boundaries**.
- Ensure that whole partitions are **less than or equal to the flash memory size**.
- Avoid **overlapping address ranges** between *independent* partitions.
- Flash addresses from `0x00000000`–`0x0000FFFF` are fixed for **Boot2** and **partition tables**, application partitions start after `0x00010000`.  
- Use **unique partition names** that are less than 8 characters.


---

## Using a Custom Partition Table

To use the custom partition configuration:

1. Create a new `4M_KV.toml` file in this project directory.
2. Set the override variable in your configuration file:

   ```bash
   CONFIG_OVERRIDE_PTFILE =4M_KV.toml
   ```
3. Rebuild the project:
   ```bash
   make
   ```

---

**End of Document**
