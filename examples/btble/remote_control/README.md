# Remote_control

This sample description ble peripheral function with HOGP GATT service.


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

## Shell Commands

This remote control example provides shell commands for simulating various remote control key presses. These commands can be used to test HID over GATT Protocol (HOGP) functionality.

### Existing Commands

The following basic navigation commands are already implemented:

```bash
ble_key_ch_ins     # Channel increase
ble_key_ch_des     # Channel decrease
ble_key_right      # Right arrow key
ble_key_left       # Left arrow key
ble_key_down       # Down arrow key
ble_key_up         # Up arrow key
ble_key_menu       # Menu button
ble_key_pwr        # Power button
ble_key_pick       # Pick/Select button
ble_key_mute       # Mute button
ble_key_home       # Home button
ble_key_vol_ins    # Volume increase
ble_key_vol_des    # Volume decrease
ble_key_back       # Back button
```

### Usage Examples

1. **Basic Navigation Control**:
   ```bash
   ble_key_up        # Move cursor up
   ble_key_down      # Move cursor down
   ble_key_left      # Move cursor left
   ble_key_right     # Move cursor right
   ```

2. **Media Control**:
   ```bash
   ble_key_vol_ins   # Increase volume
   ble_key_vol_des   # Decrease volume
   ble_key_mute      # Toggle mute
   ```

3. **System Control**:
   ```bash
   ble_key_pwr       # Power on/off
   ble_key_menu      # Open menu
   ble_key_home      # Go to home screen
   ble_key_back      # Go back
   ```

4. **Channel Control**:
   ```bash
   ble_key_ch_ins    # Next channel
   ble_key_ch_des    # Previous channel
   ```

### HID Report Mapping

Each command sends a specific HID report to simulate the corresponding key press. The reports are sent via the HID Service characteristic and can be received by any HID-compatible host device (e.g., Android TV, smart TV, etc.).